#include <dolphin/os.h>

/* L1 Data Cache Operations */
void DCInvalidateRange(void* addr, u32 nBytes) { /* Stub */ }
void DCFlushRange(void* addr, u32 nBytes) { /* Stub */ }
void DCStoreRange(void* addr, u32 nBytes) { /* Stub */ }
void DCFlushRangeNoSync(void* addr, u32 nBytes) { /* Stub */ }
void DCStoreRangeNoSync(void* addr, u32 nBytes) { /* Stub */ }
void DCZeroRange(void* addr, u32 nBytes) { /* Stub */ }
void DCTouchRange(void* addr, u32 nBytes) { /* Stub */ }
void DCFlashInvalidate(void) { /* Stub */ }
void DCEnable(void) { /* Stub */ }
void DCDisable(void) { /* Stub */ }
void DCFreeze(void) { /* Stub */ }
void DCUnfreeze(void) { /* Stub */ }
void DCTouchLoad(void* addr) { /* Stub */ }
void DCBlockZero(void* addr) { /* Stub */ }
void DCBlockStore(void* addr) { /* Stub */ }
void DCBlockFlush(void* addr) { /* Stub */ }
void DCBlockInvalidate(void* addr) { /* Stub */ }

/* L1 Instruction Cache Operations */
void ICInvalidateRange(void* addr, u32 nBytes) { /* Stub */ }
void ICSync(void) { /* Stub */ }
void ICFlashInvalidate(void) { /* Stub */ }
void ICEnable(void) { /* Stub */ }
void ICDisable(void) { /* Stub */ }
void ICFreeze(void) { /* Stub */ }
void ICUnfreeze(void) { /* Stub */ }
void ICBlockInvalidate(void* addr) { /* Stub */ }

/* L2 Cache Operations */
void L2Enable(void) { /* Stub */ }
void L2Disable(void) { /* Stub */ }
void L2GlobalInvalidate(void) { /* Stub */ }
void L2SetDataOnly(BOOL dataOnly) { /* Stub */ }
void L2SetWriteThrough(BOOL writeThrough) { /* Stub */ }

/* Locked Cache Operations */
void LCEnable(void) { /* Stub */ }
void LCDisable(void) { /* Stub */ }
void LCLoadBlocks(void* destTag, void* srcAddr, u32 numBlocks) { /* Stub */ }
void LCStoreBlocks(void* destAddr, void* srcTag, u32 numBlocks) { /* Stub */ }
u32  LCLoadData(void* destAddr, void* srcAddr, u32 nBytes) { return 0; }
u32  LCStoreData(void* destAddr, void* srcAddr, u32 nBytes) { return 0; }
u32  LCQueueLength(void) { return 0; }
void LCQueueWait(u32 len) { /* Stub */ }
void LCFlushQueue(void) { /* Stub */ }
void LCAlloc(void* addr, u32 nBytes) { /* Stub */ }
void LCAllocNoInvalidate(void* addr, u32 nBytes) { /* Stub */ }
void LCAllocOneTag(BOOL invalidate, void* tag) { /* Stub */ }
void LCAllocTags(BOOL invalidate, void* startTag, u32 numBlocks) { /* Stub */ }

