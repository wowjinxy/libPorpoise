#ifndef SIM_ARAM_H
#define SIM_ARAM_H

#include <dolphin/types.h>
#include <dolphin/ar.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SIM_ARAMStartDMA(u32 type, uintptr_t mainmem_addr, u32 aram_addr, u32 length, ARCallback arCallback, ARQCallback arqCallback, uintptr_t arqData);


#ifdef __cplusplus
}
#endif

#endif