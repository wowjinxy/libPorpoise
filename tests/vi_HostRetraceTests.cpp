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
extern "C" void __VIHostInitRuntime(void);

namespace {

std::array<int, 7> EventOrder = {};
size_t EventCount = 0;
u32 PreRetraceCount = 0;
u32 PostRetraceCount = 0;
u32 CopyClearCount = 0;
u32 AlarmCount = 0;
u32 RenderCount = 0;
SDL_threadID RenderThread = 0;
std::atomic<bool> WorkerEntered = false;
std::atomic<bool> WorkerWoke = false;

void RecordEvent(int event) {
    if (EventCount < EventOrder.size()) {
        EventOrder[EventCount++] = event;
    }
}

void RecordPreRetrace(u32 retraceCount) {
    PreRetraceCount = retraceCount;
    RecordEvent(1);
}

void RecordPostRetrace(u32 retraceCount) {
    PostRetraceCount = retraceCount;
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

}

extern "C" void SIM_VIInit(void) {
}

extern "C" void SIM_Render(void) {
    ++RenderCount;
    RenderThread = SDL_ThreadID();
    RecordEvent(3);
}

extern "C" void __GXHostApplyCopyClear(void) {
    ++CopyClearCount;
}

int main() {
    __VIHostInitRuntime();
    VISetNextFrameBuffer(reinterpret_cast<void*>(0x1000));
    VIFlush();

    const SDL_threadID ownerThread = SDL_ThreadID();
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

    const u32 afterWaitRetraceCount = VIGetRetraceCount();
    const u32 afterWaitField = VIGetNextField();
    __VIHostOnCopyTex();
    __VIHostOnCopyDisp();
    const u32 afterIntermediateCopyRetraceCount = VIGetRetraceCount();
    __VIHostOnCopyTex();
    __VIHostOnDraw();
    __VIHostOnCopyDisp();
    const u32 afterCopyRetraceCount = VIGetRetraceCount();
    const u32 afterCopyField = VIGetNextField();
    VIWaitForRetrace();

    VISetPreRetraceCallback(previousPre);
    VISetPostRetraceCallback(previousPost);

    const u32 expectedWaitRetraceCount = initialRetraceCount + 1u;
    const u32 expectedCopyRetraceCount = initialRetraceCount + 2u;
    return
        afterWaitRetraceCount == expectedWaitRetraceCount &&
        afterIntermediateCopyRetraceCount == expectedWaitRetraceCount &&
        afterCopyRetraceCount == expectedCopyRetraceCount &&
        VIGetRetraceCount() == expectedCopyRetraceCount &&
        initialLine == 0u &&
        initialField != afterWaitField &&
        afterWaitField != afterCopyField &&
        VIGetNextField() == afterCopyField &&
        PreRetraceCount == expectedCopyRetraceCount &&
        PostRetraceCount == expectedCopyRetraceCount &&
        AlarmCount == 1u &&
        RenderCount == 2u &&
        RenderThread == ownerThread &&
        WorkerWoke.load(std::memory_order_acquire) &&
        CopyClearCount == 1u &&
        EventCount == EventOrder.size() &&
        EventOrder[0] == 0 &&
        EventOrder[1] == 1 &&
        EventOrder[2] == 2 &&
        EventOrder[3] == 3 &&
        EventOrder[4] == 1 &&
        EventOrder[5] == 2 &&
        EventOrder[6] == 3
            ? 0
            : 1;
}
