#include <dolphin/vi.h>
#include <dolphin/os.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_timer.h>

#include <array>
#include <atomic>
#include <cstddef>

extern "C" void __VIHostOnCopyDisp(void);
extern "C" void __VIHostOnCopyTex(void);
extern "C" void __VIHostOnDraw(void);
namespace {

std::array<int, 11> EventOrder = {};
size_t EventCount = 0;
u32 PreRetraceCount = 0;
u32 PostRetraceCount = 0;
u32 CopyClearCount = 0;
u32 AlarmCount = 0;
u32 RenderCount = 0;
SDL_threadID RenderThread = 0;
std::array<SDL_threadID, 4> PreRetraceThreads = {};
std::array<SDL_threadID, 4> PostRetraceThreads = {};
std::array<SDL_threadID, 3> RenderThreads = {};
size_t PreCallbackCount = 0;
size_t PostCallbackCount = 0;
bool CopyDisplayInPost = false;
SDL_threadID SimulatedContextThread = 0;
SDL_threadID ContextReleaseThread = 0;
SDL_threadID ContextAcquireThread = 0;
SDL_threadID HandoffThread = 0;
u32 ContextReleaseCount = 0;
u32 ContextReleaseAttemptCount = 0;
u32 ContextAcquireCount = 0;
std::atomic<bool> AllowContextRelease = true;
std::atomic<bool> WorkerEntered = false;
std::atomic<bool> WorkerWoke = false;
std::atomic<bool> HandoffAdopted = false;
std::atomic<bool> HandoffIsOwner = false;
OSMessageQueue HandoffDoneQueue = {};
OSMessage HandoffDoneMessages[1] = {};
OSMessageQueue HandoffExitQueue = {};
OSMessage HandoffExitMessages[1] = {};

void RecordEvent(int event) {
    if (EventCount < EventOrder.size()) {
        EventOrder[EventCount++] = event;
    }
}

void RecordPreRetrace(u32 retraceCount) {
    PreRetraceCount = retraceCount;
    if (PreCallbackCount < PreRetraceThreads.size()) {
        PreRetraceThreads[PreCallbackCount] = SDL_ThreadID();
    }
    ++PreCallbackCount;
    RecordEvent(1);
}

void RecordPostRetrace(u32 retraceCount) {
    PostRetraceCount = retraceCount;
    if (PostCallbackCount < PostRetraceThreads.size()) {
        PostRetraceThreads[PostCallbackCount] = SDL_ThreadID();
    }
    ++PostCallbackCount;
    if (CopyDisplayInPost) {
        CopyDisplayInPost = false;
        __VIHostOnCopyDisp();
    }
    RecordEvent(2);
}

void RecordAlarm(OSAlarm*, OSContext*) {
    ++AlarmCount;
    RecordEvent(0);
}

int WaitForRetrace(void*) {
    WorkerEntered.store(true, std::memory_order_release);
    VIWaitForRetrace();
    WorkerWoke.store(true, std::memory_order_release);
    return 0;
}

int AdoptRenderThread(void*) {
    HandoffThread = SDL_ThreadID();
    const BOOL adopted = __VIHostAdoptRenderThread();
    const BOOL adoptedAgain = adopted && __VIHostAdoptRenderThread();
    HandoffAdopted.store(adoptedAgain, std::memory_order_release);
    HandoffIsOwner.store(
        adoptedAgain && __VIHostIsRenderThread(),
        std::memory_order_release);
    if (adoptedAgain) {
        VIWaitForRetrace();
    }
    OSSendMessage(
        &HandoffDoneQueue,
        reinterpret_cast<OSMessage>(1),
        OS_MESSAGE_NOBLOCK);
    if (adoptedAgain) {
        OSMessage exitMessage = nullptr;
        OSReceiveMessage(
            &HandoffExitQueue,
            &exitMessage,
            OS_MESSAGE_BLOCK);
    }
    return adoptedAgain ? 0 : 1;
}

}

extern "C" void SIM_VIInit(void) {
}

extern "C" void SIM_Render(void) {
    if (RenderCount < RenderThreads.size()) {
        RenderThreads[RenderCount] = SDL_ThreadID();
    }
    ++RenderCount;
    RenderThread = SDL_ThreadID();
    RecordEvent(3);
}

extern "C" BOOL SIM_HostReleaseRenderContext(void) {
    const SDL_threadID currentThread = SDL_ThreadID();
    ++ContextReleaseAttemptCount;
    if (SimulatedContextThread != currentThread) {
        return FALSE;
    }
    if (!AllowContextRelease.load(std::memory_order_acquire)) {
        return FALSE;
    }
    ++ContextReleaseCount;
    ContextReleaseThread = currentThread;
    SimulatedContextThread = 0;
    return TRUE;
}

extern "C" BOOL SIM_HostAcquireRenderContext(void) {
    const SDL_threadID currentThread = SDL_ThreadID();
    if (SimulatedContextThread != 0 &&
        SimulatedContextThread != currentThread) {
        return FALSE;
    }
    if (SimulatedContextThread != currentThread) {
        ++ContextAcquireCount;
        ContextAcquireThread = currentThread;
        SimulatedContextThread = currentThread;
    }
    return TRUE;
}

extern "C" void __GXHostApplyCopyClear(void) {
    ++CopyClearCount;
}

int main() {
    __VIHostInitRuntime();
    const SDL_threadID ownerThread = SDL_ThreadID();
    SimulatedContextThread = ownerThread;
    if (!__VIHostIsRenderThread() || !__VIHostAdoptRenderThread()) {
        return 2;
    }
    VISetNextFrameBuffer(reinterpret_cast<void*>(0x1000));
    VIFlush();

    const u32 initialField = VIGetNextField();
    const u32 initialLine = VIGetCurrentLine();
    const u32 initialRetraceCount = VIGetRetraceCount();
    const VIRetraceCallback previousPre =
        VISetPreRetraceCallback(RecordPreRetrace);
    const VIRetraceCallback previousPost =
        VISetPostRetraceCallback(RecordPostRetrace);

    OSAlarm alarm = {};
    OSCreateAlarm(&alarm);
    OSSetAlarm(&alarm, 0, RecordAlarm);

    SDL_Thread* worker =
        SDL_CreateThread(WaitForRetrace, "VI retrace waiter", nullptr);
    if (worker == nullptr) {
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
    SDL_Delay(10);
    if (VIGetRetraceCount() != initialRetraceCount || RenderCount != 0u) {
        return 4;
    }

    VIWaitForRetrace();
    SDL_WaitThread(worker, nullptr);
    const Uint64 firstPacedRetrace = SDL_GetPerformanceCounter();

    const u32 afterWaitRetraceCount = VIGetRetraceCount();
    const u32 afterWaitField = VIGetNextField();
    const u32 afterWaitRenderCount = RenderCount;
    const size_t afterWaitPreCallbackCount = PreCallbackCount;
    const size_t afterWaitPostCallbackCount = PostCallbackCount;
    __VIHostOnCopyTex();
    __VIHostOnCopyDisp();
    const u32 afterIntermediateCopyRetraceCount = VIGetRetraceCount();
    __VIHostOnCopyTex();
    __VIHostOnDraw();
    __VIHostOnCopyDisp();
    const Uint64 secondPacedRetrace = SDL_GetPerformanceCounter();
    const u32 afterCopyRetraceCount = VIGetRetraceCount();
    const u32 afterCopyField = VIGetNextField();
    VIWaitForRetrace();

    CopyDisplayInPost = true;
    VIWaitForRetrace();
    const Uint64 thirdPacedRetrace = SDL_GetPerformanceCounter();
    const u32 afterCallbackCopyRetraceCount = VIGetRetraceCount();
    const u32 afterCallbackCopyField = VIGetNextField();
    const u32 afterCallbackCopyRenderCount = RenderCount;

    OSInitMessageQueue(
        &HandoffDoneQueue,
        HandoffDoneMessages,
        1);
    OSInitMessageQueue(
        &HandoffExitQueue,
        HandoffExitMessages,
        1);

    /* A release failure wakes the requester and leaves the old owner intact. */
    AllowContextRelease.store(false, std::memory_order_release);
    SDL_Thread* failedHandoffWorker =
        SDL_CreateThread(AdoptRenderThread, "failed VI handoff", nullptr);
    if (failedHandoffWorker == nullptr) {
        return 5;
    }
    OSMessage failedHandoffMessage = nullptr;
    if (!OSReceiveMessage(
            &HandoffDoneQueue,
            &failedHandoffMessage,
            OS_MESSAGE_BLOCK)) {
        return 6;
    }
    int failedHandoffStatus = 0;
    SDL_WaitThread(failedHandoffWorker, &failedHandoffStatus);
    if (failedHandoffStatus == 0 ||
        HandoffAdopted.load(std::memory_order_acquire) ||
        HandoffIsOwner.load(std::memory_order_acquire) ||
        !__VIHostIsRenderThread() ||
        SimulatedContextThread != ownerThread) {
        return 7;
    }

    AllowContextRelease.store(true, std::memory_order_release);
    SDL_Thread* handoffWorker =
        SDL_CreateThread(AdoptRenderThread, "VI render handoff", nullptr);
    if (handoffWorker == nullptr) {
        return 8;
    }
    OSMessage handoffMessage = nullptr;
    if (!OSReceiveMessage(
            &HandoffDoneQueue,
            &handoffMessage,
            OS_MESSAGE_BLOCK)) {
        return 9;
    }
    const u32 afterHandoffRetraceCount = VIGetRetraceCount();
    const u32 afterHandoffField = VIGetNextField();

    VISetPreRetraceCallback(previousPre);
    VISetPostRetraceCallback(previousPost);

    const u32 expectedWaitRetraceCount = initialRetraceCount + 1u;
    const u32 expectedCopyRetraceCount = initialRetraceCount + 2u;
    const u32 expectedCallbackCopyRetraceCount = initialRetraceCount + 3u;
    const u32 expectedHandoffRetraceCount = initialRetraceCount + 4u;
    const Uint64 pacingFrequency = SDL_GetPerformanceFrequency();
    /* Two NTSC retraces span approximately 33.367 ms. The pacer may retain
     * one interval of credit after scheduler overshoot, so validate the
     * sustained ceiling across both intervals rather than requiring each
     * individual host wakeup to be perfectly uniform. */
    const Uint64 minimumPacedSpan =
        pacingFrequency * 33u / 1000u;
    const bool passed =
        afterWaitRetraceCount == expectedWaitRetraceCount &&
        afterWaitRenderCount == 0u &&
        afterWaitPreCallbackCount == 1u &&
        afterWaitPostCallbackCount == 1u &&
        afterIntermediateCopyRetraceCount == expectedWaitRetraceCount &&
        afterCopyRetraceCount == expectedCopyRetraceCount &&
        afterCallbackCopyRetraceCount == expectedCallbackCopyRetraceCount &&
        afterCallbackCopyRenderCount == 2u &&
        thirdPacedRetrace - firstPacedRetrace >= minimumPacedSpan &&
        afterHandoffRetraceCount == expectedHandoffRetraceCount &&
        VIGetRetraceCount() == expectedHandoffRetraceCount &&
        initialLine == 0u &&
        initialField != afterWaitField &&
        afterWaitField != afterCopyField &&
        afterCopyField != afterCallbackCopyField &&
        afterCallbackCopyField != afterHandoffField &&
        VIGetNextField() == afterHandoffField &&
        PreRetraceCount == expectedHandoffRetraceCount &&
        PostRetraceCount == expectedHandoffRetraceCount &&
        PreCallbackCount == 4u &&
        PostCallbackCount == 4u &&
        PreRetraceThreads[0] == ownerThread &&
        PreRetraceThreads[1] == ownerThread &&
        PreRetraceThreads[2] == ownerThread &&
        PreRetraceThreads[3] == HandoffThread &&
        PostRetraceThreads[0] == ownerThread &&
        PostRetraceThreads[1] == ownerThread &&
        PostRetraceThreads[2] == ownerThread &&
        PostRetraceThreads[3] == HandoffThread &&
        AlarmCount == 1u &&
        RenderCount == 2u &&
        RenderThreads[0] == ownerThread &&
        RenderThreads[1] == ownerThread &&
        RenderThread == ownerThread &&
        WorkerWoke.load(std::memory_order_acquire) &&
        HandoffAdopted.load(std::memory_order_acquire) &&
        HandoffIsOwner.load(std::memory_order_acquire) &&
        !__VIHostIsRenderThread() &&
        handoffMessage == reinterpret_cast<OSMessage>(1) &&
        failedHandoffMessage == reinterpret_cast<OSMessage>(1) &&
        ContextReleaseAttemptCount == 2u &&
        ContextReleaseCount == 1u &&
        ContextAcquireCount == 1u &&
        ContextReleaseThread == ownerThread &&
        ContextAcquireThread == HandoffThread &&
        SimulatedContextThread == HandoffThread &&
        CopyClearCount == 1u &&
        EventCount == EventOrder.size() &&
        EventOrder[0] == 0 &&
        EventOrder[1] == 1 &&
        EventOrder[2] == 2 &&
        EventOrder[3] == 1 &&
        EventOrder[4] == 2 &&
        EventOrder[5] == 3 &&
        EventOrder[6] == 1 &&
        EventOrder[7] == 2 &&
        EventOrder[8] == 3 &&
        EventOrder[9] == 1 &&
        EventOrder[10] == 2;
    OSSendMessage(
        &HandoffExitQueue,
        reinterpret_cast<OSMessage>(1),
        OS_MESSAGE_NOBLOCK);
    int handoffStatus = 1;
    SDL_WaitThread(handoffWorker, &handoffStatus);
    return passed && handoffStatus == 0 ? 0 : 1;
}
