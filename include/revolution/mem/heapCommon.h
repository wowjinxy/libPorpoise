#ifndef REVOLUTION_MEM_HEAP_COMMON_H
#define REVOLUTION_MEM_HEAP_COMMON_H

#include <stddef.h>
#include <revolution/os.h>
#include <revolution/mem/list.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEM_HEAP_INVALID_HANDLE NULL
#define MEM_HEAP_DEFAULT_ALIGNMENT 4

#define MEM_HEAP_OPT_0_CLEAR     (1 << 0)
#define MEM_HEAP_OPT_DEBUG_FILL  (1 << 1)
#define MEM_HEAP_OPT_THREAD_SAFE (1 << 2)

#define MEM_HEAP_ERROR_PRINT (1 << 0)

#define MEMi_EXPHEAP_SIGNATURE ((u32)0x45585048)
#define MEMi_FRMHEAP_SIGNATURE ((u32)0x46524D48)
#define MEMi_UNTHEAP_SIGNATURE ((u32)0x554E5448)

typedef enum MEMHeapType {
    MEM_HEAP_TYPE_EXP,
    MEM_HEAP_TYPE_FRM,
    MEM_HEAP_TYPE_UNIT,
    MEM_HEAP_TYPE_UNKNOWN
} MEMHeapType;

enum {
    MEM_HEAP_FILL_NOUSE,
    MEM_HEAP_FILL_ALLOC,
    MEM_HEAP_FILL_FREE,
    MEM_HEAP_FILL_MAX
};

typedef struct MEMiHeapHead {
    u32 signature;
    MEMLink link;
    MEMList childList;
    void* heapStart;
    void* heapEnd;
    OSMutex mutex;
    union {
        u32 val;
        struct {
            u32 reserved : 24;
            u32 optFlag : 8;
        } fields;
    } attribute;
} MEMiHeapHead;

typedef MEMiHeapHead* MEMHeapHandle;

MEMHeapHandle MEMFindContainHeap(const void* memBlock);
MEMHeapHandle MEMFindParentHeap(MEMHeapHandle heap);

static inline void* MEMGetHeapStartAddress(MEMHeapHandle heap)
{
    return heap ? (void*)heap : NULL;
}

static inline void* MEMGetHeapEndAddress(MEMHeapHandle heap)
{
    return heap ? heap->heapEnd : NULL;
}

static inline s32 MEMGetHeapTotalUsableSize(MEMHeapHandle heap)
{
    return heap ? (s32)((u8*)heap->heapEnd - (u8*)heap->heapStart) : 0;
}

static inline s32 MEMGetHeapTotalSize(MEMHeapHandle heap)
{
    return heap ? (s32)((u8*)heap->heapEnd - (u8*)heap) : 0;
}

static inline MEMHeapType MEMGetHeapType(MEMHeapHandle heap)
{
    if (!heap)
        return MEM_HEAP_TYPE_UNKNOWN;
    switch (heap->signature) {
    case MEMi_EXPHEAP_SIGNATURE:
        return MEM_HEAP_TYPE_EXP;
    case MEMi_FRMHEAP_SIGNATURE:
        return MEM_HEAP_TYPE_FRM;
    case MEMi_UNTHEAP_SIGNATURE:
        return MEM_HEAP_TYPE_UNIT;
    default:
        return MEM_HEAP_TYPE_UNKNOWN;
    }
}

static inline BOOL MEMIsExpHeap(MEMHeapHandle heap)
{
    return heap && heap->signature == MEMi_EXPHEAP_SIGNATURE;
}

static inline BOOL MEMIsFrmHeap(MEMHeapHandle heap)
{
    return heap && heap->signature == MEMi_FRMHEAP_SIGNATURE;
}

static inline BOOL MEMIsUnitHeap(MEMHeapHandle heap)
{
    return heap && heap->signature == MEMi_UNTHEAP_SIGNATURE;
}

#if !defined(_DEBUG)
#define MEMDumpHeap(heap) ((void)0)
#define MEMSetFillValForHeap(type, value) (0)
#define MEMGetFillValForHeap(type) (0)
#else
void MEMDumpHeap(MEMHeapHandle heap);
u32 MEMSetFillValForHeap(int type, u32 value);
u32 MEMGetFillValForHeap(int type);
#endif

#ifdef __cplusplus
}
#endif

#endif /* REVOLUTION_MEM_HEAP_COMMON_H */
