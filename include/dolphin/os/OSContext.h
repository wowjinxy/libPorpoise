#ifndef DOLPHIN_OSCONTEXT_H
#define DOLPHIN_OSCONTEXT_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// Floating point context modes
#define OS_CONTEXT_MODE_FPU  0x01u
#define OS_CONTEXT_MODE_PSFP 0x02u

// Context status
#define OS_CONTEXT_STATE_FPSAVED 0x01u
#define OS_CONTEXT_STATE_EXC     0x02u

typedef struct OSContext
{
    u32 gpr[32];
    u32 cr;
    u32 lr;
    u32 ctr;
    u32 xer;
    f64 fpr[32];
    u32 fpscr_pad;
    u32 fpscr;
    u32 srr0;
    u32 srr1;
    u16 mode;
    u16 state;
    u32 gqr[8];
    u32 psf_pad;
    f64 psf[32];
} OSContext;

u32        OSGetStackPointer   (void);
u32        OSSwitchStack       (u32 newsp);
int        OSSwitchFiber       (u32 pc, u32 newsp);
int        OSSwitchFiberEx     (u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 pc, u32 newsp);
void       OSSetCurrentContext (OSContext* context);
OSContext* OSGetCurrentContext (void);
u32        OSSaveContext       (OSContext* context);
void       OSLoadContext       (OSContext* context);
void       OSClearContext      (OSContext* context);
void       OSInitContext       (OSContext* context, u32 pc, u32 sp);
void       OSLoadFPUContext    (OSContext* context);
void       OSSaveFPUContext    (OSContext* context);
void       OSFillFPUContext    (OSContext* context);
void       OSDumpContext       (OSContext* context);

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OSCONTEXT_H */

