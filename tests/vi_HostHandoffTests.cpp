#include <dolphin/os.h>
#include <dolphin/vi.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_timer.h>

#include <array>
#include <atomic>
#include <cstddef>

extern "C" void __VIHostOnCopyDisp(void);

namespace {

SDL_threadID InitialOwner = 0;
SDL_threadID WorkerThread = 0;
SDL_threadID ContextThread = 0;
SDL_threadID LastReleaseThread = 0;
SDL_threadID LastAcquireThread = 0;
std::array<SDL_threadID, 4> RenderThreads = {};
std::array<SDL_threadID, 4> PreThreads = {};
std::array<SDL_threadID, 4> PostThreads = {};
std::array<u32, 2> SuccessfulReleaseRenderCounts = {};
u32 RenderCount = 0;
u32 PreCount = 0;
u32 PostCount = 0;
u32 ReleaseAttemptCount = 0;
u32 ReleaseCount = 0;
u32 AcquireAttemptCount = 0;
u32 AcquireCount = 0;
u32 OwnerAlarmCount = 0;
SDL_threadID OwnerAlarmThread = 0;
OSAlarm OwnerAlarm = {};
std::atomic<bool> FailNextRelease = false;
std::atomic<bool> FailNextAcquire = false;
std::atomic<bool> RequestEntered = false;
std::atomic<bool> RequestAdopted = false;
std::atomic<bool> RequestIsOwner = false;
std::atomic<bool> SpawnRequestInPre = false;
std::atomic<bool> SpawnFailed = false;
std::atomic<bool> RenderWithoutContext = false;
OSMessageQueue DoneQueue = {};
OSMessage DoneMessages[1] = {};
OSMessageQueue ExitQueue = {};
OSMessage ExitMessages[1] = {};
SDL_Thread* ActiveWorker = nullptr;

void RecordOwnerAlarm(OSAlarm*, OSContext*) {
    ++OwnerAlarmCount;
    OwnerAlarmThread = SDL_ThreadID();
}

bool WaitUntilRequestPublished() {
    const Uint64 deadline = SDL_GetTicks64() + 1000u;
    while (SDL_GetTicks64() < deadline) {
        if (RequestEntered.load(std::memory_order_acquire) &&
            !__VIHostIsRenderThread()) {
            return true;
        }
        SDL_Delay(1);
    }
    return false;
}

int AdoptWorker(void*) {
    WorkerThread = SDL_ThreadID();
    RequestEntered.store(true, std::memory_order_release);
    const BOOL adopted = __VIHostAdoptRenderThread();
    const BOOL adoptedAgain = adopted && __VIHostAdoptRenderThread();
    RequestAdopted.store(adoptedAgain, std::memory_order_release);
    RequestIsOwner.store(
        adoptedAgain && __VIHostIsRenderThread(),
        std::memory_order_release);
    if (adoptedAgain) {
        OSCreateAlarm(&OwnerAlarm);
        OSSetAlarm(&OwnerAlarm, 0, RecordOwnerAlarm);
        VIWaitForRetrace();
    }
    OSSendMessage(
        &DoneQueue,
        reinterpret_cast<OSMessage>(1),
        OS_MESSAGE_NOBLOCK);
    if (adoptedAgain) {
        OSMessage exitMessage = nullptr;
        OSReceiveMessage(
            &ExitQueue,
            &exitMessage,
            OS_MESSAGE_BLOCK);
    }
    return adoptedAgain ? 0 : 1;
}

SDL_Thread* StartAdoption(const char* name) {
    RequestEntered.store(false, std::memory_order_release);
    RequestAdopted.store(false, std::memory_order_release);
    RequestIsOwner.store(false, std::memory_order_release);
    return SDL_CreateThread(AdoptWorker, name, nullptr);
}

void RecordPre(u32) {
    if (PreCount < PreThreads.size()) {
        PreThreads[PreCount] = SDL_ThreadID();
    }
    ++PreCount;

    if (SpawnRequestInPre.exchange(false, std::memory_order_acq_rel)) {
        ActiveWorker = StartAdoption("active-retrace VI handoff");
        if (ActiveWorker == nullptr || !WaitUntilRequestPublished()) {
            SpawnFailed.store(true, std::memory_order_release);
        }
    }
}

void RecordPost(u32) {
    if (PostCount < PostThreads.size()) {
        PostThreads[PostCount] = SDL_ThreadID();
    }
    ++PostCount;
}

bool ReceiveDone() {
    OSMessage message = nullptr;
    return OSReceiveMessage(
               &DoneQueue,
               &message,
               OS_MESSAGE_BLOCK) &&
           message == reinterpret_cast<OSMessage>(1);
}

}  // namespace

extern "C" void SIM_VIInit(void) {
}

extern "C" void SIM_Render(void) {
    const SDL_threadID currentThread = SDL_ThreadID();
    if (ContextThread != currentThread) {
        RenderWithoutContext.store(true, std::memory_order_release);
    }
    if (RenderCount < RenderThreads.size()) {
        RenderThreads[RenderCount] = currentThread;
    }
    ++RenderCount;
}

extern "C" BOOL SIM_HostReleaseRenderContext(void) {
    const SDL_threadID currentThread = SDL_ThreadID();
    ++ReleaseAttemptCount;
    if (ContextThread != currentThread) {
        return FALSE;
    }
    if (FailNextRelease.exchange(false, std::memory_order_acq_rel)) {
        return FALSE;
    }
    if (ReleaseCount < SuccessfulReleaseRenderCounts.size()) {
        SuccessfulReleaseRenderCounts[ReleaseCount] = RenderCount;
    }
    ++ReleaseCount;
    LastReleaseThread = currentThread;
    ContextThread = 0;
    return TRUE;
}

extern "C" BOOL SIM_HostAcquireRenderContext(void) {
    const SDL_threadID currentThread = SDL_ThreadID();
    if (ContextThread == currentThread) {
        return TRUE;
    }
    ++AcquireAttemptCount;
    if (FailNextAcquire.exchange(false, std::memory_order_acq_rel)) {
        return FALSE;
    }
    if (ContextThread != 0) {
        return FALSE;
    }
    ++AcquireCount;
    LastAcquireThread = currentThread;
    ContextThread = currentThread;
    return TRUE;
}

extern "C" void __GXHostApplyCopyClear(void) {
}

int main() {
    OSInit();
    __VIHostInitRuntime();
    InitialOwner = SDL_ThreadID();
    ContextThread = InitialOwner;
    if (!__VIHostIsRenderThread() ||
        !__VIHostAdoptRenderThread()) {
        return 1;
    }

    VIInit();
    if (!__VIHostIsRenderThread() ||
        !__VIHostAdoptRenderThread()) {
        return 2;
    }

    OSInitMessageQueue(&DoneQueue, DoneMessages, 1);
    OSInitMessageQueue(&ExitQueue, ExitMessages, 1);
    const VIRetraceCallback previousPre =
        VISetPreRetraceCallback(RecordPre);
    const VIRetraceCallback previousPost =
        VISetPostRetraceCallback(RecordPost);
    const u32 initialRetraceCount = VIGetRetraceCount();

    /* Release failure restores and wakes an owner already inside VIWait. */
    FailNextRelease.store(true, std::memory_order_release);
    SDL_Thread* releaseFailureWorker =
        StartAdoption("release-failure VI handoff");
    if (releaseFailureWorker == nullptr || !WaitUntilRequestPublished()) {
        return 3;
    }
    OSCheckAlarmQueue();
    if (ReleaseAttemptCount != 0u || ContextThread != InitialOwner) {
        return 4;
    }
    VIWaitForRetrace();
    int releaseFailureStatus = 0;
    SDL_WaitThread(releaseFailureWorker, &releaseFailureStatus);
    if (!ReceiveDone() || releaseFailureStatus == 0 ||
        !__VIHostIsRenderThread() || ContextThread != InitialOwner) {
        return 5;
    }

    /* A one-shot requester acquire failure rolls GL back to the old owner. */
    FailNextAcquire.store(true, std::memory_order_release);
    SDL_Thread* acquireFailureWorker =
        StartAdoption("acquire-failure VI handoff");
    if (acquireFailureWorker == nullptr || !WaitUntilRequestPublished()) {
        return 6;
    }
    VIWaitForRetrace();
    int acquireFailureStatus = 0;
    SDL_WaitThread(acquireFailureWorker, &acquireFailureStatus);
    if (!ReceiveDone() || acquireFailureStatus == 0 ||
        !__VIHostIsRenderThread() || ContextThread != InitialOwner) {
        return 7;
    }

    /* Request from a pre-callback; old presentation must precede release. */
    SpawnRequestInPre.store(true, std::memory_order_release);
    __VIHostOnCopyDisp();
    if (SpawnFailed.load(std::memory_order_acquire) ||
        ActiveWorker == nullptr || !ReceiveDone()) {
        return 8;
    }

    const u32 finalRetraceCount = VIGetRetraceCount();
    /* Only the explicit display copy presents; handoff retraces retain it. */
    const bool passed =
        RequestAdopted.load(std::memory_order_acquire) &&
        RequestIsOwner.load(std::memory_order_acquire) &&
        !__VIHostIsRenderThread() &&
        ContextThread == WorkerThread &&
        finalRetraceCount == initialRetraceCount + 4u &&
        RenderCount == 1u &&
        PreCount == 4u &&
        PostCount == 4u &&
        RenderThreads[0] == InitialOwner &&
        PreThreads[0] == InitialOwner &&
        PreThreads[1] == InitialOwner &&
        PreThreads[2] == InitialOwner &&
        PreThreads[3] == WorkerThread &&
        PostThreads[0] == InitialOwner &&
        PostThreads[1] == InitialOwner &&
        PostThreads[2] == InitialOwner &&
        PostThreads[3] == WorkerThread &&
        !RenderWithoutContext.load(std::memory_order_acquire) &&
        ReleaseAttemptCount == 3u &&
        ReleaseCount == 2u &&
        SuccessfulReleaseRenderCounts[0] == 0u &&
        SuccessfulReleaseRenderCounts[1] == 1u &&
        LastReleaseThread == InitialOwner &&
        AcquireAttemptCount == 3u &&
        AcquireCount == 2u &&
        LastAcquireThread == WorkerThread &&
        OwnerAlarmCount == 1u &&
        OwnerAlarmThread == WorkerThread;

    OSSendMessage(
        &ExitQueue,
        reinterpret_cast<OSMessage>(1),
        OS_MESSAGE_NOBLOCK);
    int successStatus = 1;
    SDL_WaitThread(ActiveWorker, &successStatus);
    VISetPreRetraceCallback(previousPre);
    VISetPostRetraceCallback(previousPost);
    return passed && successStatus == 0 ? 0 : 9;
}
