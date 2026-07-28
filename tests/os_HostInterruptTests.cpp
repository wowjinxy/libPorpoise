#include <dolphin/os.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_timer.h>

#include <atomic>

namespace {

std::atomic<bool> ContenderMayRun = false;
std::atomic<bool> ContenderAttempting = false;
std::atomic<bool> ContenderEntered = false;

std::atomic<bool> WakerMayRun = false;
std::atomic<bool> WakerEntered = false;
OSThreadQueue WaitQueue = {};

bool WaitFor(const std::atomic<bool>& value, int timeoutMilliseconds) {
    for (int elapsed = 0; elapsed < timeoutMilliseconds; ++elapsed) {
        if (value.load(std::memory_order_acquire)) {
            return true;
        }
        SDL_Delay(1);
    }
    return value.load(std::memory_order_acquire);
}

int CompeteForScheduler(void*) {
    while (!ContenderMayRun.load(std::memory_order_acquire)) {
        SDL_Delay(1);
    }

    ContenderAttempting.store(true, std::memory_order_release);
    const BOOL enabled = OSDisableInterrupts();
    ContenderEntered.store(true, std::memory_order_release);
    OSRestoreInterrupts(enabled);
    return 0;
}

int WakeSleepingThread(void*) {
    while (!WakerMayRun.load(std::memory_order_acquire)) {
        SDL_Delay(1);
    }

    const BOOL enabled = OSDisableInterrupts();
    WakerEntered.store(true, std::memory_order_release);
    OSWakeupThread(&WaitQueue);
    OSRestoreInterrupts(enabled);
    return 0;
}

}

extern "C" void SIM_VIInit(void) {
}

extern "C" void SIM_Render(void) {
}

int main() {
    /*
     * Nested disables must retain one process-wide scheduler lock until the
     * matching outer restore enables interrupts again.
     */
    const BOOL outerEnabled = OSDisableInterrupts();
    const BOOL nestedEnabled = OSDisableInterrupts();
    if (!outerEnabled || nestedEnabled) {
        OSRestoreInterrupts(outerEnabled);
        return 1;
    }

    SDL_Thread* contender =
        SDL_CreateThread(CompeteForScheduler, "OS scheduler contender", nullptr);
    if (contender == nullptr) {
        OSRestoreInterrupts(outerEnabled);
        return 2;
    }

    ContenderMayRun.store(true, std::memory_order_release);
    if (!WaitFor(ContenderAttempting, 1000)) {
        OSRestoreInterrupts(outerEnabled);
        SDL_WaitThread(contender, nullptr);
        return 3;
    }
    SDL_Delay(20);
    if (ContenderEntered.load(std::memory_order_acquire)) {
        OSRestoreInterrupts(outerEnabled);
        SDL_WaitThread(contender, nullptr);
        return 4;
    }

    if (OSRestoreInterrupts(nestedEnabled) != FALSE) {
        OSRestoreInterrupts(outerEnabled);
        SDL_WaitThread(contender, nullptr);
        return 5;
    }
    SDL_Delay(20);
    if (ContenderEntered.load(std::memory_order_acquire)) {
        OSRestoreInterrupts(outerEnabled);
        SDL_WaitThread(contender, nullptr);
        return 6;
    }

    if (OSRestoreInterrupts(outerEnabled) != FALSE ||
        !WaitFor(ContenderEntered, 1000)) {
        SDL_WaitThread(contender, nullptr);
        return 7;
    }
    SDL_WaitThread(contender, nullptr);

    /*
     * A sleeping emulated thread must surrender the scheduler lock so its
     * waker can run, then resume with its prior interrupt state intact.
     */
    OSInitThreadQueue(&WaitQueue);
    const BOOL waitOuterEnabled = OSDisableInterrupts();
    SDL_Thread* waker =
        SDL_CreateThread(WakeSleepingThread, "OS scheduler waker", nullptr);
    if (waker == nullptr) {
        OSRestoreInterrupts(waitOuterEnabled);
        return 8;
    }

    WakerMayRun.store(true, std::memory_order_release);
    OSSleepThread(&WaitQueue);
    const BOOL enabledAfterWait = OSDisableInterrupts();
    OSRestoreInterrupts(enabledAfterWait);
    OSRestoreInterrupts(waitOuterEnabled);
    SDL_WaitThread(waker, nullptr);

    return WakerEntered.load(std::memory_order_acquire) &&
                   !enabledAfterWait
               ? 0
               : 9;
}
