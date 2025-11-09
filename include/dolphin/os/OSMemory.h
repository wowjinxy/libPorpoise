#ifndef DOLPHIN_OSMEMORY_H
#define DOLPHIN_OSMEMORY_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Memory size APIs */
u32 OSGetPhysicalMem1Size(void);
u32 OSGetPhysicalMem2Size(void);
u32 OSGetConsoleSimulatedMem1Size(void);
u32 OSGetConsoleSimulatedMem2Size(void);

#define OSGetPhysicalMemSize            OSGetPhysicalMem1Size
#define OSGetConsoleSimulatedMemSize    OSGetConsoleSimulatedMem1Size

/* Memory protection APIs */
#define OS_PROTECT_CHAN0            0
#define OS_PROTECT_CHAN1            1
#define OS_PROTECT_CHAN2            2
#define OS_PROTECT_CHAN3            3

#define OS_PROTECT_CONTROL_NONE     0x00
#define OS_PROTECT_CONTROL_READ     0x01
#define OS_PROTECT_CONTROL_WRITE    0x02
#define OS_PROTECT_CONTROL_RDWR     (OS_PROTECT_CONTROL_READ | OS_PROTECT_CONTROL_WRITE)

#define OS_PROTECT0_BIT             0x00000001
#define OS_PROTECT1_BIT             0x00000002
#define OS_PROTECT2_BIT             0x00000004
#define OS_PROTECT3_BIT             0x00000008
#define OS_PROTECT_ADDRERR_BIT      0x00000010

void OSProtectRange(u32 chan, void* addr, u32 nBytes, u32 control);

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OSMEMORY_H */

