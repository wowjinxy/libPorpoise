#ifndef REVOLUTION_MEM_EXP_HEAP_H
#define REVOLUTION_MEM_EXP_HEAP_H

#include <revolution/mem/heapCommon.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MEM_EXPHEAP_ALLOC_DIR_FRONT,
    MEM_EXPHEAP_ALLOC_DIR_REAR
};

enum {
    MEM_EXPHEAP_ALLOC_MODE_FIRST,
    MEM_EXPHEAP_ALLOC_MODE_NEAR
};

typedef struct MEMiExpHeapMBlockHead {
    u16 signature;
    union {
        u16 val;
        struct {
            u16 allocDir : 1;
            u16 alignment : 7;
            u16 groupID : 8;
        } fields;
    } attribute;
    u32 blockSize;
    struct MEMiExpHeapMBlockHead* pMBHeadPrev;
    struct MEMiExpHeapMBlockHead* pMBHeadNext;
} MEMiExpHeapMBlockHead;

typedef struct MEMiExpMBlockList {
    MEMiExpHeapMBlockHead* head;
    MEMiExpHeapMBlockHead* tail;
} MEMiExpMBlockList;

typedef struct MEMiExpHeapHead {
    MEMiExpMBlockList mbFreeList;
    MEMiExpMBlockList mbUsedList;
    u16 groupID;
    union {
        u16 val;
        struct {
            u16 reserved : 14;
            u16 useMarginOfAlign : 1;
            u16 allocMode : 1;
        } fields;
    } feature;
} MEMiExpHeapHead;

typedef void (*MEMHeapVisitor)(void* memBlock, MEMHeapHandle heap, u32 userParam);

MEMHeapHandle MEMCreateExpHeapEx(void* startAddress, u32 size, u16 optFlag);
void* MEMDestroyExpHeap(MEMHeapHandle heap);
void* MEMAllocFromExpHeapEx(MEMHeapHandle heap, u32 size, int alignment);
u32 MEMResizeForMBlockExpHeap(MEMHeapHandle heap, void* memBlock, u32 size);
void MEMFreeToExpHeap(MEMHeapHandle heap, void* memBlock);
u32 MEMGetTotalFreeSizeForExpHeap(MEMHeapHandle heap);
u32 MEMGetAllocatableSizeForExpHeapEx(MEMHeapHandle heap, int alignment);
BOOL MEMiIsEmptyExpHeap(MEMHeapHandle heap);
u16 MEMSetAllocModeForExpHeap(MEMHeapHandle heap, u16 mode);
u16 MEMGetAllocModeForExpHeap(MEMHeapHandle heap);
BOOL MEMUseMarginOfAlignmentForExpHeap(MEMHeapHandle heap, BOOL reuse);
u16 MEMSetGroupIDForExpHeap(MEMHeapHandle heap, u16 groupID);
u16 MEMGetGroupIDForExpHeap(MEMHeapHandle heap);
void MEMVisitAllocatedForExpHeap(MEMHeapHandle heap, MEMHeapVisitor visitor, u32 userParam);
u32 MEMGetSizeForMBlockExpHeap(const void* memBlock);
u16 MEMGetGroupIDForMBlockExpHeap(const void* memBlock);
u16 MEMGetAllocDirForMBlockExpHeap(const void* memBlock);
u32 MEMAdjustExpHeap(MEMHeapHandle heap);
BOOL MEMCheckExpHeap(MEMHeapHandle heap, u32 optFlag);
BOOL MEMCheckForMBlockExpHeap(const void* memBlock, MEMHeapHandle heap, u32 optFlag);

static inline MEMHeapHandle MEMCreateExpHeap(void* startAddress, u32 size)
{
    return MEMCreateExpHeapEx(startAddress, size, 0);
}

static inline void* MEMAllocFromExpHeap(MEMHeapHandle heap, u32 size)
{
    return MEMAllocFromExpHeapEx(heap, size, MEM_HEAP_DEFAULT_ALIGNMENT);
}

static inline u32 MEMGetAllocatableSizeForExpHeap(MEMHeapHandle heap)
{
    return MEMGetAllocatableSizeForExpHeapEx(heap, MEM_HEAP_DEFAULT_ALIGNMENT);
}

#ifdef __cplusplus
}
#endif

#endif /* REVOLUTION_MEM_EXP_HEAP_H */
