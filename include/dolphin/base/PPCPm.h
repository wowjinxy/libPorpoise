/*---------------------------------------------------------------------------*
  PPCPm.h - Performance Monitor API for libPorpoise
  
  PC-compatible version of the base library's performance monitor API.
 *---------------------------------------------------------------------------*/

#ifndef DOLPHIN_BASE_PPCPM_H
#define DOLPHIN_BASE_PPCPM_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void PMBegin(void);
void PMEnd(void);

u32  PMCycles(void);
u32  PML1FetchMisses(void);
u32  PML1MissCycles(void);
u32  PMInstructions(void);

#ifdef __cplusplus
}
#endif

#endif  /* DOLPHIN_BASE_PPCPM_H */

