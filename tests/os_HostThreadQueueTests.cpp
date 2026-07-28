#include <dolphin/os.h>
#include <SDL2/SDL_timer.h>

#include <atomic>
#include <cstdint>

namespace {

OSThreadQueue Queue = {};
OSThread Worker = {};
OSThread ExplicitExitWorker = {};
OSThread DetachedWorker = {};
OSMutex OwnedMutex = {};
std::atomic<bool> WorkerEntered = false;
std::atomic<bool> WorkerWoke = false;
std::atomic<bool> DetachedFunctionReturned = false;

void* WaitForWake(void*) {
    OSLockMutex(&OwnedMutex);
    WorkerEntered.store(true, std::memory_order_release);
    OSSleepThread(&Queue);
    WorkerWoke.store(true, std::memory_order_release);
    // The host exit hook must release this owned OS mutex.
    return reinterpret_cast<void*>(static_cast<uintptr_t>(0x1234));
}

void* ExitExplicitly(void*) {
    OSExitThread(
        reinterpret_cast<void*>(static_cast<uintptr_t>(0x5678)));
    // Host C functions cannot be forcibly unwound, but the wrapper must retain
    // the explicit OSExitThread value when this function subsequently returns.
    return reinterpret_cast<void*>(static_cast<uintptr_t>(0x9999));
}

void* ExitDetached(void*) {
    DetachedFunctionReturned.store(true, std::memory_order_release);
    return nullptr;
}

}

extern "C" void SIM_VIInit(void) {
}

extern "C" void SIM_Render(void) {
}

int main() {
    OSInitThreadQueue(&Queue);
    OSInitMutex(&OwnedMutex);
    if (!OSCreateThread(&Worker, WaitForWake, nullptr, nullptr, 0, 16, 0)) {
        return 1;
    }
    if (OSResumeThread(&Worker) != 1) {
        return 2;
    }

    for (int elapsed = 0;
         elapsed < 100 && !WorkerEntered.load(std::memory_order_acquire);
         ++elapsed) {
        SDL_Delay(1);
    }
    if (!WorkerEntered.load(std::memory_order_acquire)) {
        return 3;
    }

    bool foundWaitingMembership = false;
    for (int elapsed = 0; elapsed < 1000; ++elapsed) {
        const BOOL enabled = OSDisableInterrupts();
        foundWaitingMembership =
            Worker.state == OS_THREAD_STATE_WAITING &&
            Worker.queue == &Queue &&
            Queue.head == &Worker &&
            Queue.tail == &Worker;
        OSRestoreInterrupts(enabled);
        if (foundWaitingMembership) {
            break;
        }
        SDL_Delay(1);
    }
    if (!foundWaitingMembership) {
        return 4;
    }

    SDL_Delay(10);
    OSWakeupThread(&Queue);

    void* result = nullptr;
    if (!OSJoinThread(&Worker, &result)) {
        return 5;
    }
    if (!WorkerWoke.load(std::memory_order_acquire) ||
        result != reinterpret_cast<void*>(static_cast<uintptr_t>(0x1234)) ||
        Worker.state != OS_THREAD_STATE_NULL ||
        Queue.head != nullptr ||
        Queue.tail != nullptr) {
        return 6;
    }

    if (!OSTryLockMutex(&OwnedMutex)) {
        return 7;
    }
    OSUnlockMutex(&OwnedMutex);

    if (!OSCreateThread(
            &ExplicitExitWorker,
            ExitExplicitly,
            nullptr,
            nullptr,
            0,
            16,
            0) ||
        OSResumeThread(&ExplicitExitWorker) != 1) {
        return 8;
    }
    result = nullptr;
    if (!OSJoinThread(&ExplicitExitWorker, &result) ||
        result != reinterpret_cast<void*>(static_cast<uintptr_t>(0x5678))) {
        return 9;
    }

    if (!OSCreateThread(
            &DetachedWorker,
            ExitDetached,
            nullptr,
            nullptr,
            0,
            16,
            OS_THREAD_ATTR_DETACH) ||
        OSResumeThread(&DetachedWorker) != 1) {
        return 10;
    }
    bool detachedCleanly = false;
    for (int elapsed = 0; elapsed < 1000; ++elapsed) {
        const BOOL enabled = OSDisableInterrupts();
        detachedCleanly =
            DetachedWorker.state == OS_THREAD_STATE_NULL &&
            DetachedWorker.sdlThread == nullptr;
        OSRestoreInterrupts(enabled);
        if (detachedCleanly) {
            break;
        }
        SDL_Delay(1);
    }
    return DetachedFunctionReturned.load(std::memory_order_acquire) &&
                   detachedCleanly
               ? 0
               : 11;
}
