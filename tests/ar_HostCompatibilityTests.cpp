#include <dolphin/ar.h>
#include <dolphin/os/OSHostAddress.h>
#include <dolphin/os/OSHostMemory.h>

#include <array>
#include <cstdint>
#include <cstring>

namespace {

int DirectCompletionCount = 0;
u32 DirectCompletionStatus = ~0U;
ARDMAResult DirectCompletionResult = AR_DMA_RESULT_NOT_STARTED;

int QueueCompletionCount = 0;
u32 QueueCompletionStatus = ~0U;
ARDMAResult QueueCompletionResult = AR_DMA_RESULT_NOT_STARTED;
u32 QueueCallbackToken = 0;
ARQRequest* QueueCallbackRequest = nullptr;

void DirectCompletion() {
    ++DirectCompletionCount;
    DirectCompletionStatus = ARGetDMAStatus();
    DirectCompletionResult = ARGetLastDMAResult();
}

void QueueCompletion(u32 requestAddress) {
    ++QueueCompletionCount;
    QueueCompletionStatus = ARGetDMAStatus();
    QueueCompletionResult = ARGetLastDMAResult();
    QueueCallbackToken = requestAddress;
    QueueCallbackRequest = static_cast<ARQRequest*>(
        __OSHostDecodeAddress(requestAddress));
}

void FillPattern(void* destination, u8 seed, u32 length) {
    auto* bytes = static_cast<u8*>(destination);
    for (u32 index = 0; index < length; ++index) {
        bytes[index] = static_cast<u8>(seed + index * 3U);
    }
}

}

extern "C" void SIM_VIInit(void) {
}

extern "C" void SIM_Render(void) {
}

int main() {
    constexpr u32 TransferLength = 64;
    constexpr u32 StrictAddress = 0x80010000U;
    constexpr u32 AramAddress = 0x00004000U;
    constexpr u32 QueueAramAddress = 0x00008000U;

    const OSHostMemoryLayout* layout =
        __OSHostMemoryInit(OS_HOST_MEMORY_PROFILE_GAMECUBE);
    if (layout == nullptr) {
        return 1;
    }

    u32 stackEntries[4] = {};
    if (ARInit(stackEntries, 4) != __AR_ARAM_USR_BASE_ADDR ||
        !ARCheckInit() ||
        ARGetSize() != 0x01000000U ||
        ARGetDMAStatus() != 0 ||
        ARGetLastDMAResult() != AR_DMA_RESULT_NOT_STARTED) {
        return 2;
    }

    auto* strictBuffer = reinterpret_cast<u8*>(
        static_cast<std::uintptr_t>(StrictAddress));
    std::array<u8, TransferLength> expected = {};
    FillPattern(expected.data(), 0x21, TransferLength);
    std::memcpy(strictBuffer, expected.data(), TransferLength);

    /* Preflight is exact and has no completion/status side effects. */
    ARRegisterDMACallback(DirectCompletion);
    if (ARValidateDMA(
            2,
            StrictAddress,
            AramAddress,
            TransferLength) != AR_DMA_RESULT_INVALID_DIRECTION ||
        ARValidateDMA(
            ARAM_DIR_MRAM_TO_ARAM,
            StrictAddress,
            AramAddress + 1U,
            TransferLength) != AR_DMA_RESULT_INVALID_ALIGNMENT ||
        ARValidateDMA(
            ARAM_DIR_MRAM_TO_ARAM,
            StrictAddress + 1U,
            AramAddress,
            TransferLength) != AR_DMA_RESULT_INVALID_ALIGNMENT ||
        ARValidateDMA(
            ARAM_DIR_MRAM_TO_ARAM,
            StrictAddress,
            0x00FFFFE0U,
            TransferLength) != AR_DMA_RESULT_INVALID_ARAM_RANGE ||
        ARValidateDMA(
            ARAM_DIR_MRAM_TO_ARAM,
            0x817FFFE0U,
            AramAddress,
            TransferLength) != AR_DMA_RESULT_INVALID_MAIN_MEMORY_RANGE ||
        DirectCompletionCount != 0 ||
        ARGetDMAStatus() != 0 ||
        ARGetLastDMAResult() != AR_DMA_RESULT_NOT_STARTED) {
        return 3;
    }

    if (ARStartDMAEx(
        ARAM_DIR_MRAM_TO_ARAM,
        StrictAddress,
        AramAddress,
        TransferLength) != AR_DMA_RESULT_SUCCESS) {
        return 4;
    }
    if (DirectCompletionCount != 1 ||
        DirectCompletionStatus != 0 ||
        DirectCompletionResult != AR_DMA_RESULT_SUCCESS ||
        ARGetLastDMAResult() != AR_DMA_RESULT_SUCCESS ||
        ARGetDMAStatus() != 0) {
        return 5;
    }

    std::memset(strictBuffer, 0, TransferLength);
    ARStartDMA(
        ARAM_DIR_ARAM_TO_MRAM,
        StrictAddress,
        AramAddress,
        TransferLength);
    if (DirectCompletionCount != 2 ||
        DirectCompletionResult != AR_DMA_RESULT_SUCCESS ||
        std::memcmp(strictBuffer, expected.data(), TransferLength) != 0) {
        return 6;
    }

    /*
     * Invalid operations still complete so callback-driven callers cannot
     * hang, and both the return value and callback-visible result are exact.
     */
    if (ARStartDMAEx(
            2,
            StrictAddress,
            AramAddress,
            TransferLength) != AR_DMA_RESULT_INVALID_DIRECTION ||
        DirectCompletionCount != 3 ||
        DirectCompletionStatus != 0 ||
        DirectCompletionResult != AR_DMA_RESULT_INVALID_DIRECTION ||
        ARGetLastDMAResult() != AR_DMA_RESULT_INVALID_DIRECTION) {
        return 7;
    }
    if (ARStartDMAEx(
            ARAM_DIR_MRAM_TO_ARAM,
            StrictAddress,
            AramAddress + 1U,
            TransferLength) != AR_DMA_RESULT_INVALID_ALIGNMENT ||
        DirectCompletionCount != 4 ||
        DirectCompletionStatus != 0 ||
        DirectCompletionResult != AR_DMA_RESULT_INVALID_ALIGNMENT) {
        return 8;
    }
    if (ARStartDMAEx(
            ARAM_DIR_MRAM_TO_ARAM,
            StrictAddress,
            0x00FFFFE0U,
            TransferLength) != AR_DMA_RESULT_INVALID_ARAM_RANGE ||
        DirectCompletionCount != 5 ||
        DirectCompletionStatus != 0 ||
        DirectCompletionResult != AR_DMA_RESULT_INVALID_ARAM_RANGE) {
        return 9;
    }
    if (ARStartDMAEx(
            ARAM_DIR_MRAM_TO_ARAM,
            0x817FFFE0U,
            AramAddress,
            TransferLength) != AR_DMA_RESULT_INVALID_MAIN_MEMORY_RANGE ||
        DirectCompletionCount != 6 ||
        DirectCompletionStatus != 0 ||
        DirectCompletionResult != AR_DMA_RESULT_INVALID_MAIN_MEMORY_RANGE) {
        return 10;
    }
    if (ARStartDMAEx(
            ARAM_DIR_MRAM_TO_ARAM,
            StrictAddress,
            AramAddress,
            TransferLength - 1U) != AR_DMA_RESULT_INVALID_ALIGNMENT ||
        DirectCompletionCount != 7 ||
        DirectCompletionStatus != 0 ||
        DirectCompletionResult != AR_DMA_RESULT_INVALID_ALIGNMENT ||
        ARGetDMAStatus() != 0) {
        return 11;
    }

    alignas(32) std::array<u8, TransferLength> hostBuffer = {};
    FillPattern(hostBuffer.data(), 0x72, TransferLength);
    expected = hostBuffer;
    const u32 hostToken = __OSHostEncodeAddress(hostBuffer.data());
    if (hostToken == 0 || !__OSHostIsAddressToken(hostToken)) {
        return 6;
    }

    ARQInit();
    if (!ARQCheckInit() ||
        ARQGetChunkSize() != ARQ_CHUNK_SIZE_DEFAULT) {
        return 7;
    }
    ARQSetChunkSize(8192);
    ARQSetChunkSize(33);
    if (ARQGetChunkSize() != 8192) {
        return 8;
    }

    ARQRequest writeRequest = {};
    QueueCallbackRequest = nullptr;
    ARQPostRequest(
        &writeRequest,
        0x1234,
        ARQ_TYPE_MRAM_TO_ARAM,
        ARQ_PRIORITY_HIGH,
        hostToken,
        QueueAramAddress,
        TransferLength,
        QueueCompletion);
    if (QueueCompletionCount != 1 ||
        QueueCompletionStatus != 0 ||
        QueueCompletionResult != AR_DMA_RESULT_SUCCESS ||
        QueueCallbackRequest != &writeRequest ||
        QueueCallbackToken == 0 ||
        __OSHostDecodeAddress(QueueCallbackToken) != nullptr ||
        writeRequest.owner != 0x1234 ||
        writeRequest.priority != ARQ_PRIORITY_HIGH) {
        return 9;
    }

    std::memset(hostBuffer.data(), 0, TransferLength);
    ARQRequest readRequest = {};
    QueueCallbackRequest = nullptr;
    ARQPostRequest(
        &readRequest,
        0x5678,
        ARQ_TYPE_ARAM_TO_MRAM,
        ARQ_PRIORITY_LOW,
        QueueAramAddress,
        hostToken,
        TransferLength,
        QueueCompletion);
    if (QueueCompletionCount != 2 ||
        QueueCompletionResult != AR_DMA_RESULT_SUCCESS ||
        QueueCallbackRequest != &readRequest ||
        readRequest.priority != ARQ_PRIORITY_LOW ||
        hostBuffer != expected) {
        return 10;
    }

    __OSHostReleaseAddress(hostToken);
    QueueCallbackRequest = nullptr;
    ARQRequest staleRequest = {};
    ARQPostRequest(
        &staleRequest,
        0,
        ARQ_TYPE_MRAM_TO_ARAM,
        ARQ_PRIORITY_HIGH,
        hostToken,
        QueueAramAddress,
        TransferLength,
        QueueCompletion);
    if (QueueCompletionCount != 3 ||
        QueueCompletionStatus != 0 ||
        QueueCompletionResult != AR_DMA_RESULT_INVALID_MAIN_MEMORY_RANGE ||
        ARGetLastDMAResult() != AR_DMA_RESULT_INVALID_MAIN_MEMORY_RANGE ||
        QueueCallbackRequest != &staleRequest ||
        __OSHostDecodeAddress(hostToken) != nullptr) {
        return 11;
    }

    ARReset();
    if (ARCheckInit() || ARGetDMAStatus() != 0 ||
        ARGetLastDMAResult() != AR_DMA_RESULT_NOT_STARTED) {
        return 12;
    }

    return 0;
}
