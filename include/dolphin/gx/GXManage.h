#ifndef DOLPHIN_GXMANAGE_H
#define DOLPHIN_GXMANAGE_H

#include <dolphin/gx/GXFifo.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*GXDrawSyncCallback)(u16 token);
typedef void (*GXDrawDoneCallback)(void);
typedef void (*GXVerifyCallback)(GXVerifyLevel level, u32 id, const char* msg);

GXFifoObj* GXInit(void* base, u32 size);
void GXAbortFrame(void);

void GXSetDrawSync(u16 token);
u16 GXReadDrawSync(void);
GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb);

GXDrawDoneCallback GXSetDrawDoneCallback(GXDrawDoneCallback cb);
void GXDrawDone(void);
void GXSetDrawDone(void);
void GXWaitDrawDone(void);

void GXFlush(void);
void GXResetWriteGatherPipe(void);
void GXPixModeSync(void);
void GXTexModeSync(void);

void GXSetVerifyLevel(GXVerifyLevel level);
GXVerifyCallback GXSetVerifyCallback(GXVerifyCallback cb);

volatile void* GXRedirectWriteGatherPipe(void* ptr);
void GXRestoreWriteGatherPipe(void);
BOOL IsWriteGatherBufferEmpty(void);

void GXSetMisc(u32 token, u32 val);

#ifdef __cplusplus
}
#endif

#endif
