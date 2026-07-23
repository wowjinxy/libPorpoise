/*---------------------------------------------------------------------------*
  Project:  libPorpoise Demo Library
  File:     DEMOInit.h
  
  Demo initialization framework header.
  
  Based on Nintendo's Revolution SDK demo library.
 *---------------------------------------------------------------------------*/

#ifndef DOLPHIN_DEMO_INIT_H
#define DOLPHIN_DEMO_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dolphin/types.h>
#include <dolphin/gx/GXStruct.h>

/*---------------------------------------------------------------------------*
 * Global Variables
 *---------------------------------------------------------------------------*/

/* Frame buffers for double buffering */
extern void*   DemoFrameBuffer1;
extern void*   DemoFrameBuffer2;
extern void*   DemoCurrentBuffer;

/* GX FIFO buffer */
extern void*         DemoFifoBuffer;
extern GXFifoObj*    DemoFifoObj;

/* Memory configuration flags */
extern u32  DemoUseMEMHeap;    /* Set to 1 before DEMOInit() to use MEM heap */
extern BOOL DemoUseMEM2XFB;    /* Set to TRUE before DEMOInit() to use MEM2 for XFB */

/* Memory allocators */
extern void* DemoAllocator1;  /* MEM1 allocator (MEMAllocator or OSHeapHandle) */
extern void* DemoAllocator2;  /* MEM2 allocator (MEMAllocator or OSHeapHandle) */

/*---------------------------------------------------------------------------*
 * Function Prototypes
 *---------------------------------------------------------------------------*/

/* Main initialization function */
void DEMOInit(GXRenderModeObj* mode);

/* Render mode configuration */
void DEMOSetRenderMode(GXRenderModeObj* mode);

/* Memory configuration */
void DEMOConfigureMem(void);

/* GX initialization */
void DEMOInitGX(void);

/* VI (Video Interface) startup */
void DEMOStartVI(void);

/* Frame rendering functions */
void DEMOBeforeRender(void);  /* Call before rendering each frame */
void DEMODoneRender(void);    /* Call after rendering each frame */
void DEMOSwapBuffers(void);   /* Swap frame buffers */

/* Utility functions */
GXRenderModeObj* DEMOGetRenderModeObj(void);  /* Get current render mode */
void* DEMOGetCurrentBuffer(void);  /* Get current frame buffer */
void DEMOReInit(GXRenderModeObj* mode);  /* Re-initialize with new render mode */

/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_DEMO_INIT_H */

