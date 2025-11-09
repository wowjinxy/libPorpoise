#include <dolphin/os.h>
#include <string.h>

static OSContext* s_currentContext = NULL;

u32 OSGetStackPointer(void) {
    // Stub: Return stack pointer
    return 0;
}

u32 OSSwitchStack(u32 newsp) {
    // Stub: Switch stack
    return 0;
}

int OSSwitchFiber(u32 pc, u32 newsp) {
    // Stub: Switch fiber
    return 0;
}

int OSSwitchFiberEx(u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 pc, u32 newsp) {
    // Stub: Switch fiber with args
    return 0;
}

void OSSetCurrentContext(OSContext* context) {
    s_currentContext = context;
}

OSContext* OSGetCurrentContext(void) {
    return s_currentContext;
}

u32 OSSaveContext(OSContext* context) {
    // Stub: Save context
    return 0;
}

void OSLoadContext(OSContext* context) {
    // Stub: Load context
}

void OSClearContext(OSContext* context) {
    if (context) {
        memset(context, 0, sizeof(OSContext));
    }
}

void OSInitContext(OSContext* context, u32 pc, u32 sp) {
    if (!context) return;
    
    memset(context, 0, sizeof(OSContext));
    context->srr0 = pc;
    context->gpr[1] = sp;
}

void OSLoadFPUContext(OSContext* context) {
    // Stub: Load FPU context
}

void OSSaveFPUContext(OSContext* context) {
    // Stub: Save FPU context
}

void OSFillFPUContext(OSContext* context) {
    // Stub: Fill FPU context
}

void OSDumpContext(OSContext* context) {
    if (!context) return;
    
    OSReport("Context Dump:\n");
    OSReport("  PC  = 0x%08X\n", context->srr0);
    OSReport("  LR  = 0x%08X\n", context->lr);
    OSReport("  SP  = 0x%08X\n", context->gpr[1]);
}

