#ifndef DOLPHIN_OSINTERRUPT_H
#define DOLPHIN_OSINTERRUPT_H

#include <dolphin/types.h>
#include <dolphin/os/OSContext.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef s16 __OSInterrupt;
typedef void (*__OSInterruptHandler)(__OSInterrupt interrupt, OSContext* context);
typedef u32 OSInterruptMask;

#define __OS_INTERRUPT_MEM_0                0
#define __OS_INTERRUPT_MEM_1                1
#define __OS_INTERRUPT_MEM_2                2
#define __OS_INTERRUPT_MEM_3                3
#define __OS_INTERRUPT_MEM_ADDRESS          4
#define __OS_INTERRUPT_DSP_AI               5
#define __OS_INTERRUPT_DSP_ARAM             6
#define __OS_INTERRUPT_DSP_DSP              7
#define __OS_INTERRUPT_AI_AI                8
#define __OS_INTERRUPT_EXI_0_EXI            9
#define __OS_INTERRUPT_EXI_0_TC             10
#define __OS_INTERRUPT_EXI_0_EXT            11
#define __OS_INTERRUPT_EXI_1_EXI            12
#define __OS_INTERRUPT_EXI_1_TC             13
#define __OS_INTERRUPT_EXI_1_EXT            14
#define __OS_INTERRUPT_EXI_2_EXI            15
#define __OS_INTERRUPT_EXI_2_TC             16
#define __OS_INTERRUPT_PI_CP                17
#define __OS_INTERRUPT_PI_PE_TOKEN          18
#define __OS_INTERRUPT_PI_PE_FINISH         19
#define __OS_INTERRUPT_PI_SI                20
#define __OS_INTERRUPT_PI_DI                21
#define __OS_INTERRUPT_PI_RSW               22
#define __OS_INTERRUPT_PI_ERROR             23
#define __OS_INTERRUPT_PI_VI                24
#define __OS_INTERRUPT_PI_DEBUG             25
#define __OS_INTERRUPT_PI_HSP               26
#define __OS_INTERRUPT_PI_ACR               27
#define __OS_INTERRUPT_MAX                  32

#define OS_INTERRUPTMASK(interrupt) (0x80000000u >> (interrupt))

__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler);
__OSInterruptHandler __OSGetInterruptHandler(__OSInterrupt interrupt);

OSInterruptMask OSGetInterruptMask  (void);
OSInterruptMask OSSetInterruptMask  (OSInterruptMask mask);
OSInterruptMask __OSMaskInterrupts  (OSInterruptMask mask);
OSInterruptMask __OSUnmaskInterrupts(OSInterruptMask mask);

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OSINTERRUPT_H */

