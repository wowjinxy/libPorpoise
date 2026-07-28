#include <dolphin/os.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_timer.h>

#include <atomic>

namespace {

OSThreadQueue AlarmSleepQueue = {};
OSAlarm ForeignCheckAlarm = {};
OSAlarm SleepAlarm = {};
OSAlarm PeriodicAlarm = {};

SDL_threadID AlarmThread = 0;
std::atomic<int> ForeignHandlerCount = 0;
std::atomic<int> SleepHandlerCount = 0;
std::atomic<int> PeriodicHandlerCount = 0;

void RecordForeignAlarm(OSAlarm*, OSContext*) {
    AlarmThread = SDL_ThreadID();
    ForeignHandlerCount.fetch_add(1, std::memory_order_release);
}

void WakeAlarmSleep(OSAlarm*, OSContext*) {
    AlarmThread = SDL_ThreadID();
    SleepHandlerCount.fetch_add(1, std::memory_order_release);
    OSWakeupThread(&AlarmSleepQueue);
}

void RecordPeriodicAlarm(OSAlarm* alarm, OSContext*) {
    const int count =
        PeriodicHandlerCount.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (count == 3) {
        OSCancelAlarm(alarm);
    }
}

int TryForeignDispatch(void*) {
    OSCheckAlarmQueue();
    return 0;
}

int ScheduleSleepingAlarm(void*) {
    SDL_Delay(10);
    OSSetAlarm(
        &SleepAlarm,
        OSMillisecondsToTicks(5),
        WakeAlarmSleep);
    return 0;
}

bool HasSubMillisecondTicks() {
    const u32 ticksPerMillisecond = OS_TIMER_CLOCK / 1000;
    const u32 start = OSGetTick();
    const Uint32 deadline = SDL_GetTicks() + 100;

    while (SDL_GetTicks() != deadline) {
        const u32 elapsed = OSGetTick() - start;
        if (elapsed > 0 && elapsed < ticksPerMillisecond) {
            return true;
        }
        if (SDL_TICKS_PASSED(SDL_GetTicks(), deadline)) {
            break;
        }
    }
    return false;
}

}

extern "C" void SIM_VIInit(void) {
}

extern "C" void SIM_Render(void) {
}

int main() {
    const SDL_threadID ownerThread = SDL_ThreadID();
    __OSHostRegisterAlarmThread();

    if (!HasSubMillisecondTicks()) {
        return 1;
    }

    /*
     * A worker may notice a due alarm, but only the designated emulated CPU
     * thread is allowed to invoke its handler.
     */
    OSCreateAlarm(&ForeignCheckAlarm);
    OSSetAlarm(&ForeignCheckAlarm, 0, RecordForeignAlarm);
    SDL_Thread* foreignChecker =
        SDL_CreateThread(TryForeignDispatch, "foreign alarm checker", nullptr);
    if (foreignChecker == nullptr) {
        return 2;
    }
    SDL_WaitThread(foreignChecker, nullptr);
    if (ForeignHandlerCount.load(std::memory_order_acquire) != 0) {
        return 3;
    }
    OSCheckAlarmQueue();
    if (ForeignHandlerCount.load(std::memory_order_acquire) != 1 ||
        AlarmThread != ownerThread) {
        return 4;
    }

    /*
     * Begin with no pending alarm. A worker adds one while the CPU thread is
     * asleep; the alarm poke must only recompute the deadline, while the alarm
     * handler's real queue wake completes OSSleepThread().
     */
    OSInitThreadQueue(&AlarmSleepQueue);
    OSCreateAlarm(&SleepAlarm);
    SDL_Thread* scheduler =
        SDL_CreateThread(ScheduleSleepingAlarm, "alarm scheduler", nullptr);
    if (scheduler == nullptr) {
        return 5;
    }
    OSSleepThread(&AlarmSleepQueue);
    SDL_WaitThread(scheduler, nullptr);
    if (SleepHandlerCount.load(std::memory_order_acquire) != 1 ||
        AlarmThread != ownerThread) {
        return 6;
    }

    /*
     * A handler that is already due when the owner attempts to block must run
     * before sleeping. Its queue wake must not be lost between the predicate
     * check and the condition wait.
     */
    OSSetAlarm(&SleepAlarm, 0, WakeAlarmSleep);
    OSSleepThread(&AlarmSleepQueue);
    if (SleepHandlerCount.load(std::memory_order_acquire) != 2) {
        return 7;
    }

    OSCreateAlarm(&PeriodicAlarm);
    OSSetPeriodicAlarm(
        &PeriodicAlarm,
        OSGetTime() + OSMillisecondsToTicks(1),
        OSMillisecondsToTicks(2),
        RecordPeriodicAlarm);
    for (int expected = 1; expected <= 3; ++expected) {
        SDL_Delay(3);
        OSCheckAlarmQueue();
        if (PeriodicHandlerCount.load(std::memory_order_acquire) != expected) {
            return 8;
        }
    }
    SDL_Delay(3);
    OSCheckAlarmQueue();
    return PeriodicHandlerCount.load(std::memory_order_acquire) == 3
               ? 0
               : 9;
}
