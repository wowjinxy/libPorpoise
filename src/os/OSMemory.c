#include <dolphin/os.h>

u32 OSGetPhysicalMem1Size(void) {
    return 24 * 1024 * 1024; // 24 MB
}

u32 OSGetPhysicalMem2Size(void) {
    return 64 * 1024 * 1024; // 64 MB
}

u32 OSGetConsoleSimulatedMem1Size(void) {
    return OSGetPhysicalMem1Size();
}

u32 OSGetConsoleSimulatedMem2Size(void) {
    return OSGetPhysicalMem2Size();
}

void OSProtectRange(u32 chan, void* addr, u32 nBytes, u32 control) {
    // Stub: Set memory protection
}

