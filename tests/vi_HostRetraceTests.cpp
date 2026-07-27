#include <dolphin/vi.h>

#include <array>
#include <cstddef>

extern "C" void __VIHostOnCopyDisp(void);

namespace {

std::array<int, 6> EventOrder = {};
size_t EventCount = 0;
u32 PreRetraceCount = 0;
u32 PostRetraceCount = 0;

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

}

extern "C" void SIM_VIInit(void) {
}

extern "C" void SIM_Render(void) {
    RecordEvent(3);
}

int main() {
    VISetNextFrameBuffer(reinterpret_cast<void*>(0x1000));
    VIFlush();

    const u32 initialRetraceCount = VIGetRetraceCount();
    const VIRetraceCallback previousPre =
        VISetPreRetraceCallback(RecordPreRetrace);
    const VIRetraceCallback previousPost =
        VISetPostRetraceCallback(RecordPostRetrace);

    VIWaitForRetrace();

    const u32 afterWaitRetraceCount = VIGetRetraceCount();
    __VIHostOnCopyDisp();
    const u32 afterCopyRetraceCount = VIGetRetraceCount();
    VIWaitForRetrace();

    VISetPreRetraceCallback(previousPre);
    VISetPostRetraceCallback(previousPost);

    const u32 expectedWaitRetraceCount = initialRetraceCount + 1u;
    const u32 expectedCopyRetraceCount = initialRetraceCount + 2u;
    return
        afterWaitRetraceCount == expectedWaitRetraceCount &&
        afterCopyRetraceCount == expectedCopyRetraceCount &&
        VIGetRetraceCount() == expectedCopyRetraceCount &&
        PreRetraceCount == expectedCopyRetraceCount &&
        PostRetraceCount == expectedCopyRetraceCount &&
        EventCount == EventOrder.size() &&
        EventOrder[0] == 1 &&
        EventOrder[1] == 2 &&
        EventOrder[2] == 3 &&
        EventOrder[3] == 1 &&
        EventOrder[4] == 2 &&
        EventOrder[5] == 3
            ? 0
            : 1;
}
