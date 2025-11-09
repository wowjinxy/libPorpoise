#include <dolphin/os.h>

static OSResetCallback s_resetCallback = NULL;
static OSPowerCallback s_powerCallback = NULL;

BOOL OSGetResetButtonState(void) {
    return FALSE;
}

OSResetCallback OSSetResetCallback(OSResetCallback callback) {
    OSResetCallback old = s_resetCallback;
    s_resetCallback = callback;
    return old;
}

OSPowerCallback OSSetPowerCallback(OSPowerCallback callback) {
    OSPowerCallback old = s_powerCallback;
    s_powerCallback = callback;
    return old;
}

BOOL OSGetResetSwitchState(void) {
    return OSGetResetButtonState();
}

