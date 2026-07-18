#ifndef __DEMO_H__
#define __DEMO_H__

#include <dolphin.h>
#include <charPipeline/texPalette.h>
#include <stdio.h>

#include <demo/DEMOPad.h>
#include <demo/DEMOPuts.h>
#include <demo/DEMOStats.h>


#ifdef __cplusplus
extern "C" {
#endif

extern void DEMOInit(GXRenderModeObj* mode);
extern void DEMOBeforeRender();
extern void DEMODoneRender();
extern void DEMOSwapBuffers();
extern GXRenderModeObj* DEMOGetRenderModeObj();
extern void* DEMOGetCurrentBuffer();

extern void DEMOEnableGPHangWorkaround(u32 timeoutFrames);
extern void DEMOReInit(GXRenderModeObj *mode);
extern void DEMOSetGPHangMetric(GXBool enable);

#if ( GX_REV == 1 || defined(EMU) )
static inline void DEMOSetTevColorIn(GXTevStageID stage,
                              GXTevColorArg a, GXTevColorArg b,
                              GXTevColorArg c, GXTevColorArg d )
    { GXSetTevColorIn(stage, a, b, c, d); }

static inline void DEMOSetTevOp(GXTevStageID stage, GXTevMode mode)
    { GXSetTevOp(stage, mode); }
#else
void DEMOSetTevColorIn(GXTevStageID stage,
                       GXTevColorArg a, GXTevColorArg b,
                       GXTevColorArg c, GXTevColorArg d);

void DEMOSetTevOp(GXTevStageID stage, GXTevMode mode);
#endif


#ifdef __cplusplus
}
#endif

#endif

