#ifndef DOLPHIN_GXFIFO_H
#define DOLPHIN_GXFIFO_H

#include <dolphin/gx/GXEnum.h>
#include <dolphin/gx/GXStruct.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSThread OSThread;
typedef void (*GXBreakPtCallback)(void);

void GXInitFifoBase(GXFifoObj* fifo, void* base, u32 size);
void GXInitFifoPtrs(GXFifoObj* fifo, void* readPtr, void* writePtr);
void GXGetFifoPtrs(GXFifoObj* fifo, void** readPtr, void** writePtr);
GXFifoObj* GXGetCPUFifo(void);
GXFifoObj* GXGetGPFifo(void);
/* Compatibility alias for typo occasionally seen in external scans. */
GXFifoObj* GXGetGPFIfo(void);
void GXSetCPUFifo(GXFifoObj* fifo);
void GXSetGPFifo(GXFifoObj* fifo);
void GXSaveCPUFifo(GXFifoObj* fifo);
void GXGetFifoStatus(GXFifoObj* fifo, GXBool* overhi, GXBool* underlow, u32* fifoCount, GXBool* cpu_write,
                     GXBool* gp_read, GXBool* fifowrap);
void GXGetGPStatus(GXBool* overhi, GXBool* underlow, GXBool* readIdle, GXBool* cmdIdle, GXBool* brkpt);
void GXInitFifoLimits(GXFifoObj* fifo, u32 hiWaterMark, u32 loWaterMark);
GXBreakPtCallback GXSetBreakPtCallback(GXBreakPtCallback cb);
void GXEnableBreakPt(void* break_pt);
void GXDisableBreakPt(void);
OSThread* GXSetCurrentGXThread(void);
OSThread* GXGetCurrentGXThread(void);

#ifdef __cplusplus
}
#endif

#endif
