#ifndef REVOLUTION_MEM_FRAME_HEAP_H
#define REVOLUTION_MEM_FRAME_HEAP_H

#include <revolution/mem/heapCommon.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEM_FRMHEAP_FREE_HEAD (1 << 0)
#define MEM_FRMHEAP_FREE_TAIL (1 << 1)
#define MEM_FRMHEAP_FREE_ALL (MEM_FRMHEAP_FREE_HEAD | MEM_FRMHEAP_FREE_TAIL)

typedef struct MEMiFrmHeapState {
    u32 tagName;
    void* headAllocator;
    void* tailAllocator;
    struct MEMiFrmHeapState* pPrevState;
} MEMiFrmHeapState;

typedef struct MEMiFrmHeapHead {
    void* headAllocator;
    void* tailAllocator;
    MEMiFrmHeapState* pState;
} MEMiFrmHeapHead;

void* MEMiGetFreeStartForFrmHeap(MEMHeapHandle heap);
void* MEMiGetFreeEndForFrmHeap(MEMHeapHandle heap);
MEMHeapHandle MEMCreateFrmHeapEx(void* startAddress, u32 size, u16 optFlag);
void* MEMDestroyFrmHeap(MEMHeapHandle heap);
void* MEMAllocFromFrmHeapEx(MEMHeapHandle heap, u32 size, int alignment);
void MEMFreeToFrmHeap(MEMHeapHandle heap, int mode);
u32 MEMGetAllocatableSizeForFrmHeapEx(MEMHeapHandle heap, int alignment);
BOOL MEMRecordStateForFrmHeap(MEMHeapHandle heap, u32 tagName);
BOOL MEMFreeByStateToFrmHeap(MEMHeapHandle heap, u32 tagName);
u32 MEMAdjustFrmHeap(MEMHeapHandle heap);
u32 MEMResizeForMBlockFrmHeap(MEMHeapHandle heap, void* memBlock, u32 newSize);

static inline MEMHeapHandle MEMCreateFrmHeap(void* startAddress, u32 size)
{
    return MEMCreateFrmHeapEx(startAddress, size, 0);
}

static inline void* MEMAllocFromFrmHeap(MEMHeapHandle heap, u32 size)
{
    return MEMAllocFromFrmHeapEx(heap, size, MEM_HEAP_DEFAULT_ALIGNMENT);
}

static inline u32 MEMGetAllocatableSizeForFrmHeap(MEMHeapHandle heap)
{
    return MEMGetAllocatableSizeForFrmHeapEx(heap, MEM_HEAP_DEFAULT_ALIGNMENT);
}

#ifdef __cplusplus
}
#endif

#endif /* REVOLUTION_MEM_FRAME_HEAP_H */
