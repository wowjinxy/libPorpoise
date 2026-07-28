#include <dolphin/os.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_timer.h>

#include <atomic>

namespace {

OSMutex Mutex = {};
OSCond Condition = {};
std::atomic<bool> Waiting = false;
std::atomic<bool> WokeWithMutex = false;

int WaitForCondition(void*) {
    OSLockMutex(&Mutex);
    OSLockMutex(&Mutex);
    Waiting.store(true, std::memory_order_release);
    OSWaitCond(&Condition, &Mutex);

    const BOOL recursiveLockWorked = OSTryLockMutex(&Mutex);
    if (recursiveLockWorked) {
        OSUnlockMutex(&Mutex);
    }
    WokeWithMutex.store(recursiveLockWorked, std::memory_order_release);
    OSUnlockMutex(&Mutex);
    OSUnlockMutex(&Mutex);
    return 0;
}

}

extern "C" void SIM_VIInit(void) {
}

extern "C" void SIM_Render(void) {
}

int main() {
    OSInitMutex(&Mutex);
    OSInitCond(&Condition);

    SDL_Thread* waiter =
        SDL_CreateThread(WaitForCondition, "OS condition waiter", nullptr);
    if (waiter == nullptr) {
        return 1;
    }

    for (int elapsed = 0;
         elapsed < 1000 && !Waiting.load(std::memory_order_acquire);
         ++elapsed) {
        SDL_Delay(1);
    }
    if (!Waiting.load(std::memory_order_acquire)) {
        return 2;
    }

    OSLockMutex(&Mutex);
    OSSignalCond(&Condition);
    OSUnlockMutex(&Mutex);
    SDL_WaitThread(waiter, nullptr);

    return WokeWithMutex.load(std::memory_order_acquire) ? 0 : 3;
}
