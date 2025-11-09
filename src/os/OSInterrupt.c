#include <dolphin/os.h>

static __OSInterruptHandler s_interruptHandlers[__OS_INTERRUPT_MAX] = {NULL};

__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler) {
    if (interrupt >= __OS_INTERRUPT_MAX) {
        return NULL;
    }
    
    __OSInterruptHandler old = s_interruptHandlers[interrupt];
    s_interruptHandlers[interrupt] = handler;
    return old;
}

__OSInterruptHandler __OSGetInterruptHandler(__OSInterrupt interrupt) {
    if (interrupt >= __OS_INTERRUPT_MAX) {
        return NULL;
    }
    
    return s_interruptHandlers[interrupt];
}

OSInterruptMask OSGetInterruptMask(void) {
    return 0xFFFFFFFF;
}

OSInterruptMask OSSetInterruptMask(OSInterruptMask mask) {
    return 0xFFFFFFFF;
}

OSInterruptMask __OSMaskInterrupts(OSInterruptMask mask) {
    return 0xFFFFFFFF;
}

OSInterruptMask __OSUnmaskInterrupts(OSInterruptMask mask) {
    return 0xFFFFFFFF;
}

