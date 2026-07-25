#ifndef REVOLUTION_MEM_ALLOCATOR_H
#define REVOLUTION_MEM_ALLOCATOR_H

#include <revolution/mem/heapCommon.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MEMAllocator MEMAllocator;
typedef void* (*MEMFuncAllocatorAlloc)(MEMAllocator* allocator, u32 size);
typedef void (*MEMFuncAllocatorFree)(MEMAllocator* allocator, void* memBlock);

typedef struct MEMAllocatorFunc {
    MEMFuncAllocatorAlloc pfAlloc;
    MEMFuncAllocatorFree pfFree;
} MEMAllocatorFunc;

struct MEMAllocator {
    const MEMAllocatorFunc* pFunc;
    void* pHeap;
    u32 heapParam1;
    u32 heapParam2;
};

void* MEMAllocFromAllocator(MEMAllocator* allocator, u32 size);
void MEMFreeToAllocator(MEMAllocator* allocator, void* memBlock);
void MEMInitAllocatorForExpHeap(MEMAllocator* allocator, MEMHeapHandle heap,
                                int alignment);
void MEMInitAllocatorForFrmHeap(MEMAllocator* allocator, MEMHeapHandle heap,
                                int alignment);
void MEMInitAllocatorForUnitHeap(MEMAllocator* allocator, MEMHeapHandle heap);
void MEMInitAllocatorForOSHeap(MEMAllocator* allocator, OSHeapHandle heap);

#ifdef __cplusplus
}
#endif

#endif /* REVOLUTION_MEM_ALLOCATOR_H */
