#include <dolphin/os.h>

u32 __OSFpscrEnableBits = 0;

static OSErrorHandler s_errorHandlers[OS_ERROR_MAX] = {NULL};

OSErrorHandler OSSetErrorHandler(OSError error, OSErrorHandler handler) {
    if (error >= OS_ERROR_MAX) {
        return NULL;
    }
    
    OSErrorHandler old = s_errorHandlers[error];
    s_errorHandlers[error] = handler;
    return old;
}

