/*---------------------------------------------------------------------------*
  Project:  libPorpoise Demo Library
  File:     DEMOStats.c
  
  Statistics display - stub implementation for compilation.
  
  Based on Nintendo's Revolution SDK demo library.
 *---------------------------------------------------------------------------*/

#include <dolphin/demo.h>

#ifdef __cplusplus
extern "C" {
#endif

GXBool DemoStatEnable = GX_FALSE;

void DEMOSetStats(DEMOStatObj* stat, u32 nstats, DEMOStatDispMode disp) {
    (void)stat; (void)nstats; (void)disp;
    // Stub - statistics display not implemented yet
}

#ifdef __cplusplus
} // extern "C"
#endif

