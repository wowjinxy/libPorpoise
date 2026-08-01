#include <dolphin/gx.h>
#include <dolphin/os.h>
#include <dolphin/vi.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_timer.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace {

SDL_threadID InitialOwner = 0;
SDL_threadID WorkerOwner = 0;
SDL_threadID ContextThread = 0;
SDL_threadID AlarmThread = 0;
OSThread* InitialGXThread = nullptr;
std::array<SDL_threadID, 4> RenderThreads = {};
std::array<SDL_threadID, 4> ReleaseThreads = {};
std::array<SDL_threadID, 4> AcquireThreads = {};
u32 RenderCount = 0;
u32 ReleaseCount = 0;
u32 AcquireCount = 0;
u32 AlarmCount = 0;
std::atomic<bool> RequestEntered = false;
std::atomic<bool> WorkerAdopted = false;
std::atomic<bool> RenderWithoutContext = false;
OSThread Worker = {};
OSMessageQueue ReadyQueue = {};
OSMessage ReadyMessages[1] = {};
OSMessageQueue ExitQueue = {};
OSMessage ExitMessages[1] = {};
OSAlarm ReturnedOwnerAlarm = {};

void RecordReturnedOwnerAlarm(OSAlarm*, OSContext*) {
    ++AlarmCount;
    AlarmThread = SDL_ThreadID();
}

void* AdoptThenExit(void*) {
    WorkerOwner = SDL_ThreadID();
    RequestEntered.store(true, std::memory_order_release);
    OSThread* previousGXThread = GXSetCurrentGXThread();
    const BOOL adopted =
        previousGXThread == InitialGXThread &&
        GXGetCurrentGXThread() == OSGetCurrentThread() &&
        __VIHostIsRenderThread();
    WorkerAdopted.store(adopted, std::memory_order_release);
    if (!adopted) {
        OSSendMessage(
            &ReadyQueue,
            reinterpret_cast<OSMessage>(2),
            OS_MESSAGE_NOBLOCK);
        return reinterpret_cast<void*>(static_cast<uintptr_t>(2));
    }

    VIWaitForRetrace();
    OSSendMessage(
        &ReadyQueue,
        reinterpret_cast<OSMessage>(1),
        OS_MESSAGE_NOBLOCK);

    OSMessage exitMessage = nullptr;
    if (!OSReceiveMessage(
            &ExitQueue,
            &exitMessage,
            OS_MESSAGE_BLOCK) ||
        exitMessage != reinterpret_cast<OSMessage>(1)) {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(3));
    }

    /* This due alarm must follow the VI/alarm lease back to the old owner. */
    OSCreateAlarm(&ReturnedOwnerAlarm);
    OSSetAlarm(&ReturnedOwnerAlarm, 0, RecordReturnedOwnerAlarm);
    return reinterpret_cast<void*>(static_cast<uintptr_t>(0x1234));
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

bool ReceiveReady() {
    OSMessage message = nullptr;
    return OSReceiveMessage(
               &ReadyQueue,
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
    if (ContextThread == 0) {
        return TRUE;
    }
    if (ContextThread != currentThread) {
        return FALSE;
    }
    if (ReleaseCount < ReleaseThreads.size()) {
        ReleaseThreads[ReleaseCount] = currentThread;
    }
    ++ReleaseCount;
    ContextThread = 0;
    return TRUE;
}

extern "C" BOOL SIM_HostAcquireRenderContext(void) {
    const SDL_threadID currentThread = SDL_ThreadID();
    if (ContextThread == currentThread) {
        return TRUE;
    }
    if (ContextThread != 0) {
        return FALSE;
    }
    if (AcquireCount < AcquireThreads.size()) {
        AcquireThreads[AcquireCount] = currentThread;
    }
    ++AcquireCount;
    ContextThread = currentThread;
    return TRUE;
}

extern "C" void __GXHostApplyCopyClear(void) {
}

int main() {
    InitialOwner = SDL_ThreadID();
    ContextThread = InitialOwner;
    OSInit();
    __VIHostInitRuntime();
    VIInit();
    InitialGXThread = OSGetCurrentThread();
    if (GXSetCurrentGXThread() != nullptr ||
        GXGetCurrentGXThread() != InitialGXThread ||
        !__VIHostIsRenderThread()) {
        return 1;
    }

    OSInitMessageQueue(&ReadyQueue, ReadyMessages, 1);
    OSInitMessageQueue(&ExitQueue, ExitMessages, 1);
    const u32 initialRetraceCount = VIGetRetraceCount();

    if (!OSCreateThread(
            &Worker,
            AdoptThenExit,
            nullptr,
            nullptr,
            0,
            16,
            0) ||
        OSResumeThread(&Worker) != 1) {
        return 2;
    }
    if (!WaitUntilRequestPublished()) {
        return 3;
    }

    /* Service the worker's adoption request on the old render owner. */
    VIWaitForRetrace();
    if (!ReceiveReady() ||
        !WorkerAdopted.load(std::memory_order_acquire)) {
        return 4;
    }

    OSSendMessage(
        &ExitQueue,
        reinterpret_cast<OSMessage>(1),
        OS_MESSAGE_NOBLOCK);

    void* workerResult = nullptr;
    if (!OSJoinThread(&Worker, &workerResult) ||
        workerResult !=
            reinterpret_cast<void*>(static_cast<uintptr_t>(0x1234))) {
        return 5;
    }

    /*
     * The worker returns without another retrace. A loading/render worker may
     * be joined before the main thread reclaims GX, so that public GX boundary
     * must complete the pending VI/context and alarm handback coherently.
     */
    OSThread* previousGXThread = GXSetCurrentGXThread();
    OSCheckAlarmQueue();

    /* Ownership-only retraces must not swap a cleared host back buffer. */
    const bool passed =
        previousGXThread == &Worker &&
        GXGetCurrentGXThread() == InitialGXThread &&
        __VIHostIsRenderThread() &&
        ContextThread == InitialOwner &&
        VIGetRetraceCount() == initialRetraceCount + 1u &&
        RenderCount == 0u &&
        !RenderWithoutContext.load(std::memory_order_acquire) &&
        ReleaseCount == 2u &&
        ReleaseThreads[0] == InitialOwner &&
        ReleaseThreads[1] == WorkerOwner &&
        AcquireCount == 2u &&
        AcquireThreads[0] == WorkerOwner &&
        AcquireThreads[1] == InitialOwner &&
        AlarmCount == 1u &&
        AlarmThread == InitialOwner;
    return passed ? 0 : 6;
}
