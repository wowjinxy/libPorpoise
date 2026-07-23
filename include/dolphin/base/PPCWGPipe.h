/*---------------------------------------------------------------------------*
  PPCWGPipe.h - Write Gather Pipe Definitions for libPorpoise
  
  PC-compatible version of the base library's write gather pipe definitions.
 *---------------------------------------------------------------------------*/

#ifndef DOLPHIN_BASE_PPCWGPIPE_H
#define DOLPHIN_BASE_PPCWGPIPE_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*
    PPC Write Gather Pipe
    
    Write Gather Pipe is defined as:
        PPCWGPipe wgpipe : <Write Gathered Address>;
    
    Then, used as:
        wgpipe.u8  = 0xff;
        wgpipe.s16 = -5;
        wgpipe.f32 = 0.10f;
 *---------------------------------------------------------------------------*/
typedef union uPPCWGPipe {
    u8  u8;
    u16 u16;
    u32 u32;
    u64 u64;
    s8  s8;
    s16 s16;
    s32 s32;
    s64 s64;
    f32 f32;
    f64 f64;
} PPCWGPipe;

#ifdef __cplusplus
}
#endif

#endif  /* DOLPHIN_BASE_PPCWGPIPE_H */

