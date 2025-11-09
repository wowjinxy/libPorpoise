#include <dolphin/os.h>
#include <stdlib.h>
#include <string.h>

volatile OSHeapHandle __OSCurrHeap = -1;

void* OSInitAlloc(void* arenaStart, void* arenaEnd, int maxHeaps) {
    // Stub: Initialize allocator
    return arenaStart;
}

OSHeapHandle OSCreateHeap(void* start, void* end) {
    // Stub: Create heap
    return 0;
}

void OSDestroyHeap(OSHeapHandle heap) {
    // Stub: Destroy heap
}

void OSAddToHeap(OSHeapHandle heap, void* start, void* end) {
    // Stub: Add memory to heap
}

OSHeapHandle OSSetCurrentHeap(OSHeapHandle heap) {
    OSHeapHandle old = __OSCurrHeap;
    __OSCurrHeap = heap;
    return old;
}

void* OSAllocFromHeap(OSHeapHandle heap, u32 size) {
    // Stub: Use system allocator for now
    return malloc(size);
}

void* OSAllocFixed(void** rstart, void** rend) {
    // Stub: Allocate fixed memory
    return NULL;
}

void OSFreeToHeap(OSHeapHandle heap, void* ptr) {
    // Stub: Use system deallocator for now
    free(ptr);
}

long OSCheckHeap(OSHeapHandle heap) {
    // Stub: Return available space
    return 0x1000000;
}

void OSDumpHeap(OSHeapHandle heap) {
    // Stub: Dump heap info
    OSReport("Heap %d (stub)\n", heap);
}

u32 OSReferentSize(void* ptr) {
    // Stub: Return allocation size
    return 0;
}

void OSVisitAllocated(OSAllocVisitor visitor) {
    // Stub: Visit all allocated blocks
}

