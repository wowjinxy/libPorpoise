#ifndef DOLPHIN_OSCACHE_H
#define DOLPHIN_OSCACHE_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* L1 Data Cache Operations */
void DCInvalidateRange  (void* addr, u32 nBytes);
void DCFlushRange       (void* addr, u32 nBytes);
void DCStoreRange       (void* addr, u32 nBytes);
void DCFlushRangeNoSync (void* addr, u32 nBytes);
void DCStoreRangeNoSync (void* addr, u32 nBytes);
void DCZeroRange        (void* addr, u32 nBytes);
void DCTouchRange       (void* addr, u32 nBytes);

/* Low-level DC Operations */
void DCFlashInvalidate  (void);
void DCEnable           (void);
void DCDisable          (void);
void DCFreeze           (void);
void DCUnfreeze         (void);
void DCTouchLoad        (void* addr);
void DCBlockZero        (void* addr);
void DCBlockStore       (void* addr);
void DCBlockFlush       (void* addr);
void DCBlockInvalidate  (void* addr);

/* L1 Instruction Cache Operations */
void ICInvalidateRange  (void* addr, u32 nBytes);
void ICSync             (void);
void ICFlashInvalidate  (void);
void ICEnable           (void);
void ICDisable          (void);
void ICFreeze           (void);
void ICUnfreeze         (void);
void ICBlockInvalidate  (void* addr);

/* L2 Cache Operations */
void L2Enable           (void);
void L2Disable          (void);
void L2GlobalInvalidate (void);
void L2SetDataOnly      (BOOL dataOnly);
void L2SetWriteThrough  (BOOL writeThrough);

/* Locked Cache Operations */
#define LC_BASE_PREFIX  0xE000
#define LC_BASE         (LC_BASE_PREFIX << 16)
#define LC_MAX_DMA_BLOCKS   128
#define LC_MAX_DMA_BYTES    (LC_MAX_DMA_BLOCKS * 32)

void LCEnable           (void);
void LCDisable          (void);
void LCLoadBlocks       (void* destTag, void* srcAddr, u32 numBlocks);
void LCStoreBlocks      (void* destAddr, void* srcTag, u32 numBlocks);
u32  LCLoadData         (void* destAddr, void* srcAddr, u32 nBytes);
u32  LCStoreData        (void* destAddr, void* srcAddr, u32 nBytes);
u32  LCQueueLength      (void);
void LCQueueWait        (u32 len);
void LCFlushQueue       (void);
void LCAlloc            (void* addr, u32 nBytes);
void LCAllocNoInvalidate(void* addr, u32 nBytes);
void LCAllocOneTag      (BOOL invalidate, void* tag);
void LCAllocTags        (BOOL invalidate, void* startTag, u32 numBlocks);

#define LCGetBase()     ((void*)LC_BASE)

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OSCACHE_H */

