#ifndef REVOLUTION_MEM_UNIT_HEAP_H
#define REVOLUTION_MEM_UNIT_HEAP_H

#include <revolution/mem/heapCommon.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MEMiUntHeapMBlockHead {
    struct MEMiUntHeapMBlockHead* pMBlkHdNext;
} MEMiUntHeapMBlockHead;

typedef struct MEMiUntMBlockList {
    MEMiUntHeapMBlockHead* head;
} MEMiUntMBlockList;

typedef struct MEMiUntHeapHead {
    MEMiUntMBlockList mbFreeList;
    u32 mBlkSize;
} MEMiUntHeapHead;

MEMHeapHandle MEMCreateUnitHeapEx(void* startAddress, u32 heapSize,
                                 u32 memBlockSize, int alignment, u16 optFlag);
void* MEMDestroyUnitHeap(MEMHeapHandle heap);
void* MEMAllocFromUnitHeap(MEMHeapHandle heap);
void MEMFreeToUnitHeap(MEMHeapHandle heap, void* memBlock);
u32 MEMCountFreeBlockForUnitHeap(MEMHeapHandle heap);
u32 MEMCalcHeapSizeForUnitHeap(u32 memBlockSize, u32 memBlockNum, int alignment);

static inline MEMHeapHandle MEMCreateUnitHeap(void* startAddress, u32 heapSize,
                                             u32 memBlockSize)
{
    return MEMCreateUnitHeapEx(startAddress, heapSize, memBlockSize,
                               MEM_HEAP_DEFAULT_ALIGNMENT, 0);
}

static inline u32 MEMGetMemBlockSizeForUnitHeap(MEMHeapHandle heap)
{
    const MEMiUntHeapHead* head =
        (const MEMiUntHeapHead*)((const u8*)heap + sizeof(MEMiHeapHead));
    return head->mBlkSize;
}

#ifdef __cplusplus
}
#endif

#endif /* REVOLUTION_MEM_UNIT_HEAP_H */
