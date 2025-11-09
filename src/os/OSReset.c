#include <dolphin/os.h>
#include <stdlib.h>

static u32 s_resetCode = 0;
static void* s_saveRegionStart = NULL;
static void* s_saveRegionEnd = NULL;

void OSRegisterShutdownFunction(OSShutdownFunctionInfo* info) {
    // Stub: Add to shutdown list
}

void OSUnregisterShutdownFunction(OSShutdownFunctionInfo* info) {
    // Stub: Remove from shutdown list
}

void OSRebootSystem(void) {
    OSReport("OSRebootSystem called\n");
    exit(0);
}

void OSShutdownSystem(void) {
    OSReport("OSShutdownSystem called\n");
    exit(0);
}

void OSRestart(u32 resetCode) {
    s_resetCode = resetCode | OS_RESETCODE_RESTART;
    OSReport("OSRestart called with code 0x%08X\n", resetCode);
    exit(0);
}

void OSReturnToMenu(void) {
    OSReport("OSReturnToMenu called\n");
    exit(0);
}

void OSReturnToDataManager(void) {
    OSReport("OSReturnToDataManager called\n");
    exit(0);
}

void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu) {
    s_resetCode = resetCode;
    OSReport("OSResetSystem called: reset=%d code=0x%08X forceMenu=%d\n", reset, resetCode, forceMenu);
    exit(0);
}

u32 OSGetResetCode(void) {
    return s_resetCode;
}

void OSGetSaveRegion(void** start, void** end) {
    if (start) *start = s_saveRegionStart;
    if (end) *end = s_saveRegionEnd;
}

void OSGetSavedRegion(void** start, void** end) {
    OSGetSaveRegion(start, end);
}

void OSSetSaveRegion(void* start, void* end) {
    s_saveRegionStart = start;
    s_saveRegionEnd = end;
}

