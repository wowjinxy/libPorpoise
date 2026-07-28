#include <dolphin/ar.h>
#include <dolphin/os.h>
#include <dolphin/os/OSHostAddress.h>
#include <dolphin/os/OSHostMemory.h>
#include <SDL2/SDL_thread.h>

#include <atomic>
#include <cstdint>
#include <vector>

namespace {

std::atomic<int> WorkerFailure = 0;
ARQRequest* ExpectedRequest = nullptr;
ARQRequest* CallbackRequest = nullptr;
u32 CallbackToken = 0;

int ExerciseAddressTranslator(void* argument) {
    const auto worker = reinterpret_cast<std::uintptr_t>(argument);
    int localValue = static_cast<int>(worker);

    for (int iteration = 0; iteration < 1000; ++iteration) {
        const u32 token = __OSHostEncodeAddress(&localValue);
        if (token == 0 ||
            __OSHostDecodeAddress(token) != &localValue) {
            WorkerFailure.store(1, std::memory_order_release);
            return 1;
        }

        __OSHostReleaseAddress(token);
        if (__OSHostDecodeAddress(token) != nullptr) {
            WorkerFailure.store(2, std::memory_order_release);
            return 2;
        }
    }
    return 0;
}

void ARQCompletion(u32 requestAddress) {
    CallbackToken = requestAddress;
    CallbackRequest = static_cast<ARQRequest*>(
        __OSHostDecodeAddress(requestAddress));
}

}

extern "C" void SIM_VIInit(void) {
}

extern "C" void SIM_Render(void) {
}

int main() {
    const OSHostMemoryLayout* layout =
        __OSHostMemoryInit(OS_HOST_MEMORY_PROFILE_GAMECUBE);
    if (layout == nullptr) {
        return 1;
    }

    if (__OSHostEncodeAddress(nullptr) != 0 ||
        __OSHostDecodeAddress(0) != nullptr) {
        return 2;
    }

    void* cached = reinterpret_cast<void*>(0x80123400ULL);
    void* uncached = reinterpret_cast<void*>(0xC0123400ULL);
    if (__OSHostEncodeAddress(cached) != 0x80123400U ||
        __OSHostEncodeAddress(uncached) != 0xC0123400U ||
        __OSHostDecodeAddress(0x80123400U) != cached ||
        __OSHostDecodeAddress(0xC0123400U) != uncached ||
        __OSHostDecodeAddress(0x00123400U) != cached) {
        return 3;
    }

    if (__OSHostIsAddressToken(0x00100000U) ||
        __OSHostIsAddressToken(0x80100000U) ||
        __OSHostIsAddressToken(0xC0100000U) ||
        __OSHostIsAddressToken(0xCC006000U) ||
        !__OSHostIsAddressToken(OS_HOST_ADDRESS_TOKEN_TAG) ||
        __OSHostDecodeAddress(0xCC006000U) != nullptr) {
        return 4;
    }

    int hostValue = 42;
    const u32 staleToken = __OSHostEncodeAddress(&hostValue);
    if (!__OSHostIsAddressToken(staleToken) ||
        __OSHostDecodeAddress(staleToken) != &hostValue) {
        return 5;
    }
    __OSHostReleaseAddress(staleToken);
    if (__OSHostDecodeAddress(staleToken) != nullptr) {
        return 6;
    }

    const u32 replacementToken = __OSHostEncodeAddress(&hostValue);
    if (replacementToken == 0 ||
        replacementToken == staleToken ||
        __OSHostDecodeAddress(replacementToken) != &hostValue) {
        return 7;
    }
    __OSHostReleaseAddress(replacementToken);

    std::vector<SDL_Thread*> workers;
    for (std::uintptr_t index = 1; index <= 8; ++index) {
        SDL_Thread* thread = SDL_CreateThread(
            ExerciseAddressTranslator,
            "host-address worker",
            reinterpret_cast<void*>(index));
        if (thread == nullptr) {
            return 8;
        }
        workers.push_back(thread);
    }
    for (SDL_Thread* thread : workers) {
        SDL_WaitThread(thread, nullptr);
    }
    if (WorkerFailure.load(std::memory_order_acquire) != 0) {
        return 9;
    }

    std::vector<int> values(OS_HOST_ADDRESS_TOKEN_SLOT_COUNT);
    std::vector<u32> tokens;
    tokens.reserve(OS_HOST_ADDRESS_TOKEN_SLOT_COUNT);
    for (u32 index = 0; index < OS_HOST_ADDRESS_TOKEN_SLOT_COUNT; ++index) {
        values[index] = static_cast<int>(index);
        const u32 token = __OSHostEncodeAddress(&values[index]);
        if (token == 0) {
            return 10;
        }
        tokens.push_back(token);
    }
    int overflowValue = 0;
    if (__OSHostEncodeAddress(&overflowValue) != 0) {
        return 11;
    }
    for (u32 index = 0; index < OS_HOST_ADDRESS_TOKEN_SLOT_COUNT; ++index) {
        if (__OSHostDecodeAddress(tokens[index]) != &values[index]) {
            return 12;
        }
    }
    for (u32 token : tokens) {
        __OSHostReleaseAddress(token);
    }
    for (u32 token : tokens) {
        if (__OSHostDecodeAddress(token) != nullptr) {
            return 13;
        }
    }

    /*
     * ARQ request callbacks are short-lived address users: the request token
     * is valid inside the callback and released as soon as it returns.
     */
    ARQRequest request = {};
    ExpectedRequest = &request;
    ARQInit();
    ARQPostRequest(
        &request,
        0,
        ARQ_TYPE_MRAM_TO_ARAM,
        ARQ_PRIORITY_HIGH,
        0x80100000U,
        0,
        32,
        ARQCompletion);
    __ARQInterruptServiceRoutine();
    if (CallbackRequest != ExpectedRequest ||
        CallbackToken == 0 ||
        __OSHostDecodeAddress(CallbackToken) != nullptr) {
        return 14;
    }

    return 0;
}
