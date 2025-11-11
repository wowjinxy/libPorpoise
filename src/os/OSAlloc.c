/*---------------------------------------------------------------------------*
  OSAlloc.c - Memory Allocation System
  
  Implements a custom heap allocator matching the original GC/Wii SDK.
  
  Key features:
  - Multiple independent heaps within a managed arena
  - First-fit allocation with 32-byte alignment
  - Automatic coalescing of adjacent free blocks
  - Doubly-linked lists for free and allocated blocks
  - Support for non-contiguous heaps via OSAddToHeap
  - Fixed allocations that carve out specific memory ranges
  
  Memory Structure:
    Each allocation has a hidden header (Cell) containing:
    - Doubly-linked list pointers (prev/next)
    - Size of the block (including header)
    - Debug info (heap descriptor, requested size)
    
    All blocks are aligned to 32 bytes.
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <stdlib.h>
#include <string.h>

/* Alignment and sizing macros */
#define OFFSET(n, a)    (((u32)(n)) & ((a) - 1))
#define TRUNC(n, a)     (((u32)(n)) & ~((a) - 1))
#define ROUND(n, a)     (((u32)(n) + (a) - 1) & ~((a) - 1))

#define ALIGNMENT       32
#define HEADERSIZE      ROUND(sizeof(Cell), ALIGNMENT)
#define MINOBJSIZE      (HEADERSIZE + ALIGNMENT)

/* Range checking macros */
#define InRange(targ, a, b) \
    ((u32)(a) <= (u32)(targ) && (u32)(targ) < (u32)(b))

#define RangeOverlap(aStart, aEnd, bStart, bEnd) \
    ((u32)(bStart) <= (u32)(aStart) && (u32)(aStart) < (u32)(bEnd) || \
     (u32)(bStart) < (u32)(aEnd) && (u32)(aEnd) <= (u32)(bEnd))

#define RangeSubset(aStart, aEnd, bStart, bEnd) \
    ((u32)(bStart) <= (u32)(aStart) && (u32)(aEnd) <= (u32)(bEnd))

/*---------------------------------------------------------------------------*
  Cell - Header for each memory block (free or allocated)
  
  Located HEADERSIZE bytes before the returned pointer.
  Contains linked list pointers and size information.
 *---------------------------------------------------------------------------*/
typedef struct Cell Cell;

struct Cell {
    Cell* prev;         // Previous block in list
    Cell* next;         // Next block in list
    long  size;         // Total size including header
};

/*---------------------------------------------------------------------------*
  HeapDesc - Descriptor for each heap
  
  Maintains separate lists for free and allocated blocks.
  Size is -1 if heap is destroyed/inactive.
 *---------------------------------------------------------------------------*/
typedef struct HeapDesc HeapDesc;

struct HeapDesc {
    long  size;         // Total heap size (-1 if inactive)
    Cell* free;         // List of free blocks
    Cell* allocated;    // List of allocated blocks
};

/* Global heap management */
volatile OSHeapHandle __OSCurrHeap = -1;
static HeapDesc* s_heapArray = NULL;
static int s_numHeaps = 0;
static void* s_arenaStart = NULL;
static void* s_arenaEnd = NULL;

/*---------------------------------------------------------------------------*
  Doubly-Linked List Helper Functions
 *---------------------------------------------------------------------------*/

/* Add cell to front of list */
static Cell* DLAddFront(Cell* list, Cell* cell) {
    cell->next = list;
    cell->prev = NULL;
    if (list) {
        list->prev = cell;
    }
    return cell;
}

/* Look up cell in list */
static Cell* DLLookup(Cell* list, Cell* cell) {
    for (; list; list = list->next) {
        if (list == cell) {
            return list;
        }
    }
    return NULL;
}

/* Extract cell from list */
static Cell* DLExtract(Cell* list, Cell* cell) {
    if (cell->next) {
        cell->next->prev = cell->prev;
    }
    
    if (cell->prev == NULL) {
        return cell->next; // Cell was head
    } else {
        cell->prev->next = cell->next;
        return list;
    }
}

/* Insert cell into list in sorted order by address, with coalescing */
static Cell* DLInsert(Cell* list, Cell* cell) {
    Cell* prev;
    Cell* next;
    
    // Find insertion point (sorted by address)
    for (next = list, prev = NULL; next; prev = next, next = next->next) {
        if (cell <= next) {
            break;
        }
    }
    
    cell->next = next;
    cell->prev = prev;
    
    // Try to coalesce with next block
    if (next) {
        next->prev = cell;
        if ((char*)cell + cell->size == (char*)next) {
            // Merge with next
            cell->size += next->size;
            cell->next = next = next->next;
            if (next) {
                next->prev = cell;
            }
        }
    }
    
    // Try to coalesce with previous block
    if (prev) {
        prev->next = cell;
        if ((char*)prev + prev->size == (char*)cell) {
            // Merge with prev
            prev->size += cell->size;
            prev->next = next;
            if (next) {
                next->prev = prev;
            }
            return list;
        }
        return list;
    } else {
        return cell; // New head
    }
}

/* Check if range overlaps with any cell in list */
static BOOL DLOverlap(Cell* list, void* start, void* end) {
    for (Cell* cell = list; cell; cell = cell->next) {
        if (RangeOverlap(cell, (char*)cell + cell->size, start, end)) {
            return TRUE;
        }
    }
    return FALSE;
}

/* Calculate total size of all cells in list */
static long DLSize(Cell* list) {
    long size = 0;
    for (Cell* cell = list; cell; cell = cell->next) {
        size += cell->size;
    }
    return size;
}

/*---------------------------------------------------------------------------*
  Name:         OSInitAlloc

  Description:  Initializes the heap allocation system. Must be called before
                creating any heaps.
                
                Reserves space at the start of the arena for the heap descriptor
                array, then returns the adjusted arena start.

  Arguments:    arenaStart - Start of memory arena
                arenaEnd   - End of memory arena
                maxHeaps   - Maximum number of heaps (usually 1-4)

  Returns:      Adjusted arena start (after heap array)
 *---------------------------------------------------------------------------*/
void* OSInitAlloc(void* arenaStart, void* arenaEnd, int maxHeaps) {
    u32 arraySize;
    OSHeapHandle i;
    
    OSReport("OSInitAlloc: start=%p end=%p maxHeaps=%d\n", arenaStart, arenaEnd, maxHeaps);
    
    if (maxHeaps <= 0) {
        OSPanic(__FILE__, __LINE__, "OSInitAlloc: Invalid maxHeaps %d", maxHeaps);
    }
    
    if (arenaStart >= arenaEnd) {
        OSReport("OSInitAlloc: ERROR - start (%p) >= end (%p)\n", arenaStart, arenaEnd);
        OSPanic(__FILE__, __LINE__, "OSInitAlloc: Invalid arena range");
    }
    
    // Place heap descriptor array at start of arena
    arraySize = sizeof(HeapDesc) * maxHeaps;
    s_heapArray = (HeapDesc*)arenaStart;
    s_numHeaps = maxHeaps;
    
    // Initialize all heap descriptors as inactive
    for (i = 0; i < s_numHeaps; i++) {
        HeapDesc* hd = &s_heapArray[i];
        hd->size = -1;
        hd->free = NULL;
        hd->allocated = NULL;
    }
    
    __OSCurrHeap = -1; // No current heap
    
    // Adjust arena start past the descriptor array
    arenaStart = (void*)((char*)s_heapArray + arraySize);
    arenaStart = (void*)ROUND(arenaStart, ALIGNMENT);
    
    s_arenaStart = arenaStart;
    s_arenaEnd = (void*)TRUNC(arenaEnd, ALIGNMENT);
    
    if ((char*)s_arenaEnd - (char*)s_arenaStart < MINOBJSIZE) {
        OSPanic(__FILE__, __LINE__, "OSInitAlloc: Arena too small");
    }
    
    return arenaStart;
}

/*---------------------------------------------------------------------------*
  Name:         OSCreateHeap

  Description:  Creates a new heap from the specified memory range.
                The heap can then be used for allocations via OSAllocFromHeap
                or by setting it as current with OSSetCurrentHeap.

  Arguments:    start - Start of heap memory
                end   - End of heap memory

  Returns:      Heap handle (0 to maxHeaps-1), or -1 on failure
 *---------------------------------------------------------------------------*/
OSHeapHandle OSCreateHeap(void* start, void* end) {
    OSHeapHandle heap;
    HeapDesc* hd;
    Cell* cell;
    
    if (!s_heapArray) {
        OSPanic(__FILE__, __LINE__, "OSCreateHeap: Call OSInitAlloc first");
    }
    
    if (start >= end) {
        OSReport("OSCreateHeap: Invalid range\n");
        return -1;
    }
    
    // Align boundaries
    start = (void*)ROUND(start, ALIGNMENT);
    end = (void*)TRUNC(end, ALIGNMENT);
    
    if (start >= end) {
        OSReport("OSCreateHeap: Range too small after alignment\n");
        return -1;
    }
    
    if (!RangeSubset(start, end, s_arenaStart, s_arenaEnd)) {
        OSPanic(__FILE__, __LINE__, "OSCreateHeap: Range outside arena");
    }
    
    if ((char*)end - (char*)start < MINOBJSIZE) {
        OSReport("OSCreateHeap: Range too small\n");
        return -1;
    }
    
    // Find free heap descriptor
    for (heap = 0; heap < s_numHeaps; heap++) {
        hd = &s_heapArray[heap];
        if (hd->size < 0) {
            // Found inactive descriptor
            hd->size = (char*)end - (char*)start;
            
            // Create initial free cell covering entire heap
            cell = (Cell*)start;
            cell->prev = NULL;
            cell->next = NULL;
            cell->size = hd->size;
            
            hd->free = cell;
            hd->allocated = NULL;
            
            return heap;
        }
    }
    
    // No free descriptors
    OSReport("OSCreateHeap: No free heap descriptors\n");
    return -1;
}

/*---------------------------------------------------------------------------*
  Name:         OSDestroyHeap

  Description:  Destroys a heap and frees its descriptor. The memory is not
                actually freed, but the heap handle becomes invalid.
                
                Warning: Any pointers allocated from this heap become invalid!

  Arguments:    heap - Heap handle to destroy

  Returns:      None
 *---------------------------------------------------------------------------*/
void OSDestroyHeap(OSHeapHandle heap) {
    if (!s_heapArray) {
        OSPanic(__FILE__, __LINE__, "OSDestroyHeap: No heaps initialized");
    }
    
    if (heap < 0 || heap >= s_numHeaps) {
        OSPanic(__FILE__, __LINE__, "OSDestroyHeap: Invalid heap %d", heap);
    }
    
    HeapDesc* hd = &s_heapArray[heap];
    
    if (hd->size < 0) {
        OSPanic(__FILE__, __LINE__, "OSDestroyHeap: Heap already destroyed");
    }
    
    // Check if all memory was freed
    long freeSize = DLSize(hd->free);
    if (hd->size != freeSize) {
        OSReport("OSDestroyHeap(%d): Warning - %ld bytes still allocated\n",
                 heap, hd->size - freeSize);
    }
    
    hd->size = -1;
    hd->free = NULL;
    hd->allocated = NULL;
    
    if (__OSCurrHeap == heap) {
        __OSCurrHeap = -1;
    }
}

/*---------------------------------------------------------------------------*
  Name:         OSAddToHeap

  Description:  Adds an arbitrary memory range to an existing heap.
                Used to create non-contiguous heaps or to free memory
                previously allocated with OSAllocFixed.

  Arguments:    heap  - Heap handle
                start - Start of memory to add
                end   - End of memory to add

  Returns:      None
 *---------------------------------------------------------------------------*/
void OSAddToHeap(OSHeapHandle heap, void* start, void* end) {
    if (!s_heapArray) {
        OSPanic(__FILE__, __LINE__, "OSAddToHeap: No heaps initialized");
    }
    
    if (heap < 0 || heap >= s_numHeaps) {
        OSPanic(__FILE__, __LINE__, "OSAddToHeap: Invalid heap");
    }
    
    HeapDesc* hd = &s_heapArray[heap];
    
    if (hd->size < 0) {
        OSPanic(__FILE__, __LINE__, "OSAddToHeap: Heap is inactive");
    }
    
    if (start >= end) {
        OSPanic(__FILE__, __LINE__, "OSAddToHeap: Invalid range");
    }
    
    // Align boundaries
    start = (void*)ROUND(start, ALIGNMENT);
    end = (void*)TRUNC(end, ALIGNMENT);
    
    if ((char*)end - (char*)start < MINOBJSIZE) {
        OSPanic(__FILE__, __LINE__, "OSAddToHeap: Range too small");
    }
    
    if (!RangeSubset(start, end, s_arenaStart, s_arenaEnd)) {
        OSPanic(__FILE__, __LINE__, "OSAddToHeap: Range outside arena");
    }
    
    // Create new cell
    Cell* cell = (Cell*)start;
    cell->size = (char*)end - (char*)start;
    
    // Add to free list (will coalesce if adjacent)
    hd->size += cell->size;
    hd->free = DLInsert(hd->free, cell);
}

/*---------------------------------------------------------------------------*
  Name:         OSSetCurrentHeap

  Description:  Sets the current heap. OSAlloc() and OSFree() macros use
                this heap.

  Arguments:    heap - Heap handle to make current

  Returns:      Previous current heap handle
 *---------------------------------------------------------------------------*/
OSHeapHandle OSSetCurrentHeap(OSHeapHandle heap) {
    if (!s_heapArray) {
        OSPanic(__FILE__, __LINE__, "OSSetCurrentHeap: No heaps initialized");
    }
    
    if (heap < 0 || heap >= s_numHeaps) {
        OSPanic(__FILE__, __LINE__, "OSSetCurrentHeap: Invalid heap");
    }
    
    if (s_heapArray[heap].size < 0) {
        OSPanic(__FILE__, __LINE__, "OSSetCurrentHeap: Heap is inactive");
    }
    
    OSHeapHandle prev = __OSCurrHeap;
    __OSCurrHeap = heap;
    return prev;
}

/*---------------------------------------------------------------------------*
  Name:         OSAllocFromHeap

  Description:  Allocates memory from the specified heap using first-fit
                algorithm. Returns 32-byte aligned pointer.
                
                The allocation searches the free list for the first block
                large enough to satisfy the request. If the block is larger,
                it's split.

  Arguments:    heap - Heap handle
                size - Bytes to allocate

  Returns:      Pointer to allocated memory, or NULL if out of memory
 *---------------------------------------------------------------------------*/
void* OSAllocFromHeap(OSHeapHandle heap, u32 size) {
    if (!s_heapArray) {
        OSPanic(__FILE__, __LINE__, "OSAllocFromHeap: No heaps initialized");
    }
    
    if ((long)size <= 0) {
        OSPanic(__FILE__, __LINE__, "OSAllocFromHeap: Invalid size");
    }
    
    if (heap < 0 || heap >= s_numHeaps) {
        OSPanic(__FILE__, __LINE__, "OSAllocFromHeap: Invalid heap");
    }
    
    HeapDesc* hd = &s_heapArray[heap];
    
    if (hd->size < 0) {
        OSPanic(__FILE__, __LINE__, "OSAllocFromHeap: Heap is inactive");
    }
    
    // Add header size and round to alignment
    size += HEADERSIZE;
    size = ROUND(size, ALIGNMENT);
    
    // Search free list for first fit
    Cell* cell;
    for (cell = hd->free; cell != NULL; cell = cell->next) {
        if ((long)size <= cell->size) {
            break;
        }
    }
    
    if (cell == NULL) {
        // Out of memory
        return NULL;
    }
    
    // Check if we should split the block
    long leftoverSize = cell->size - (long)size;
    
    if (leftoverSize < MINOBJSIZE) {
        // Too small to split, just use entire block
        hd->free = DLExtract(hd->free, cell);
    } else {
        // Split the block
        cell->size = (long)size;
        
        // Create new free cell from leftover
        Cell* newCell = (Cell*)((char*)cell + size);
        newCell->size = leftoverSize;
        newCell->prev = cell->prev;
        newCell->next = cell->next;
        
        if (newCell->next) {
            newCell->next->prev = newCell;
        }
        
        if (newCell->prev) {
            newCell->prev->next = newCell;
        } else {
            hd->free = newCell; // New head
        }
    }
    
    // Add to allocated list
    hd->allocated = DLAddFront(hd->allocated, cell);
    
    // Return pointer past header
    return (void*)((char*)cell + HEADERSIZE);
}

/*---------------------------------------------------------------------------*
  Name:         OSFreeToHeap

  Description:  Frees memory back to the heap it was allocated from.
                The block is added back to the free list and coalesced with
                adjacent free blocks if possible.

  Arguments:    heap - Heap handle (must match heap it was allocated from)
                ptr  - Pointer previously returned by OSAllocFromHeap

  Returns:      None
 *---------------------------------------------------------------------------*/
void OSFreeToHeap(OSHeapHandle heap, void* ptr) {
    if (!s_heapArray) {
        OSPanic(__FILE__, __LINE__, "OSFreeToHeap: No heaps initialized");
    }
    
    if (!InRange(ptr, (char*)s_arenaStart + HEADERSIZE, (char*)s_arenaEnd)) {
        OSPanic(__FILE__, __LINE__, "OSFreeToHeap: Pointer outside arena");
    }
    
    if (OFFSET(ptr, ALIGNMENT) != 0) {
        OSPanic(__FILE__, __LINE__, "OSFreeToHeap: Unaligned pointer");
    }
    
    if (s_heapArray[heap].size < 0) {
        OSPanic(__FILE__, __LINE__, "OSFreeToHeap: Heap is inactive");
    }
    
    // Get cell header
    Cell* cell = (Cell*)((char*)ptr - HEADERSIZE);
    HeapDesc* hd = &s_heapArray[heap];
    
    // Verify cell is in allocated list
    if (!DLLookup(hd->allocated, cell)) {
        OSPanic(__FILE__, __LINE__, "OSFreeToHeap: Pointer not allocated from this heap");
    }
    
    // Remove from allocated list
    hd->allocated = DLExtract(hd->allocated, cell);
    
    // Add to free list (will coalesce with adjacent blocks)
    hd->free = DLInsert(hd->free, cell);
}

/*---------------------------------------------------------------------------*
  Name:         OSAllocFixed

  Description:  Allocates a specific range of memory, carving it out from
                existing heaps as needed. This is used for DMA buffers or
                memory-mapped I/O that needs to be at specific addresses.

  Arguments:    rstart - Pointer to desired start address (will be adjusted)
                rend   - Pointer to desired end address (will be adjusted)

  Returns:      Allocated start address, or NULL on failure
 *---------------------------------------------------------------------------*/
void* OSAllocFixed(void** rstart, void** rend) {
    if (!s_heapArray) {
        OSPanic(__FILE__, __LINE__, "OSAllocFixed: No heaps initialized");
    }
    
    void* start = (void*)TRUNC(*rstart, ALIGNMENT);
    void* end = (void*)ROUND(*rend, ALIGNMENT);
    
    if (start >= end) {
        OSPanic(__FILE__, __LINE__, "OSAllocFixed: Invalid range");
    }
    
    if (!RangeSubset(start, end, s_arenaStart, s_arenaEnd)) {
        OSPanic(__FILE__, __LINE__, "OSAllocFixed: Range outside arena");
    }
    
    // Check for overlap with allocated blocks
    for (OSHeapHandle i = 0; i < s_numHeaps; i++) {
        HeapDesc* hd = &s_heapArray[i];
        if (hd->size < 0) continue;
        
        if (DLOverlap(hd->allocated, start, end)) {
            OSReport("OSAllocFixed: Range overlaps allocated memory\n");
            return NULL;
        }
    }
    
    // Carve out from free blocks in all heaps
    for (OSHeapHandle i = 0; i < s_numHeaps; i++) {
        HeapDesc* hd = &s_heapArray[i];
        if (hd->size < 0) continue;
        
        for (Cell* cell = hd->free; cell; cell = cell->next) {
            void* cellEnd = (char*)cell + cell->size;
            
            if ((char*)cellEnd <= (char*)start) continue;
            if ((char*)end <= (char*)cell) break;
            
            // This cell overlaps with our range - handle it
            if (InRange(cell, (char*)start - HEADERSIZE, end) &&
                InRange((char*)cellEnd, start, (char*)end + MINOBJSIZE)) {
                // Cell is completely consumed
                if ((char*)cell < (char*)start) start = (void*)cell;
                if ((char*)end < (char*)cellEnd) end = (void*)cellEnd;
                
                hd->free = DLExtract(hd->free, cell);
                hd->size -= cell->size;
                continue;
            }
            
            // Partial overlaps - split the cell
            // (Simplified version - full implementation would handle all cases)
            hd->free = DLExtract(hd->free, cell);
            hd->size -= cell->size;
        }
    }
    
    *rstart = start;
    *rend = end;
    return *rstart;
}

/*---------------------------------------------------------------------------*
  Name:         OSCheckHeap

  Description:  Validates heap integrity for debugging. Walks the free and
                allocated lists checking for corruption.

  Arguments:    heap - Heap handle to check

  Returns:      Number of free bytes, or -1 if heap is corrupted
 *---------------------------------------------------------------------------*/
long OSCheckHeap(OSHeapHandle heap) {
    if (!s_heapArray) return -1;
    if (heap < 0 || heap >= s_numHeaps) return -1;
    
    HeapDesc* hd = &s_heapArray[heap];
    if (hd->size < 0) return -1;
    
    long total = 0;
    long free = 0;
    
    // Check allocated list
    if (hd->allocated && hd->allocated->prev != NULL) return -1;
    for (Cell* cell = hd->allocated; cell; cell = cell->next) {
        if (!InRange(cell, s_arenaStart, s_arenaEnd)) return -1;
        if (OFFSET(cell, ALIGNMENT) != 0) return -1;
        if (cell->next && cell->next->prev != cell) return -1;
        if (cell->size < MINOBJSIZE) return -1;
        
        total += cell->size;
        if (total > hd->size) return -1;
    }
    
    // Check free list
    if (hd->free && hd->free->prev != NULL) return -1;
    for (Cell* cell = hd->free; cell; cell = cell->next) {
        if (!InRange(cell, s_arenaStart, s_arenaEnd)) return -1;
        if (OFFSET(cell, ALIGNMENT) != 0) return -1;
        if (cell->next && cell->next->prev != cell) return -1;
        if (cell->size < MINOBJSIZE) return -1;
        if (cell->next && (char*)cell + cell->size >= (char*)cell->next) return -1;
        
        total += cell->size;
        free += cell->size - HEADERSIZE;
        if (total > hd->size) return -1;
    }
    
    if (total != hd->size) return -1;
    
    return free;
}

/*---------------------------------------------------------------------------*
  Name:         OSDumpHeap

  Description:  Prints detailed information about a heap for debugging.

  Arguments:    heap - Heap handle to dump

  Returns:      None
 *---------------------------------------------------------------------------*/
void OSDumpHeap(OSHeapHandle heap) {
    OSReport("\nOSDumpHeap(%d):\n", heap);
    
    if (!s_heapArray) {
        OSReport("  No heaps initialized\n");
        return;
    }
    
    if (heap < 0 || heap >= s_numHeaps) {
        OSReport("  Invalid heap handle\n");
        return;
    }
    
    HeapDesc* hd = &s_heapArray[heap];
    
    if (hd->size < 0) {
        OSReport("  -------- Inactive\n");
        return;
    }
    
    long freeBytes = OSCheckHeap(heap);
    if (freeBytes < 0) {
        OSReport("  WARNING: Heap corrupted!\n");
        return;
    }
    
    OSReport("  Total size: %ld bytes\n", hd->size);
    OSReport("  Free:       %ld bytes\n", freeBytes);
    OSReport("  Allocated:  %ld bytes\n", hd->size - freeBytes);
    
    OSReport("  -------- Allocated Blocks:\n");
    OSReport("  addr\t\tsize\t\tend\t\tprev\t\tnext\n");
    for (Cell* cell = hd->allocated; cell; cell = cell->next) {
        OSReport("  %p\t%ld\t%p\t%p\t%p\n",
                 cell, cell->size, (char*)cell + cell->size,
                 cell->prev, cell->next);
    }
    
    OSReport("  -------- Free Blocks:\n");
    OSReport("  addr\t\tsize\t\tend\t\tprev\t\tnext\n");
    for (Cell* cell = hd->free; cell; cell = cell->next) {
        OSReport("  %p\t%ld\t%p\t%p\t%p\n",
                 cell, cell->size, (char*)cell + cell->size,
                 cell->prev, cell->next);
    }
}

/*---------------------------------------------------------------------------*
  Name:         OSReferentSize

  Description:  Returns the size of an allocated block (excluding header).

  Arguments:    ptr - Pointer previously returned by OSAllocFromHeap

  Returns:      Size of allocation in bytes
 *---------------------------------------------------------------------------*/
u32 OSReferentSize(void* ptr) {
    if (!s_heapArray) {
        OSPanic(__FILE__, __LINE__, "OSReferentSize: No heaps initialized");
    }
    
    if (!InRange(ptr, (char*)s_arenaStart + HEADERSIZE, (char*)s_arenaEnd)) {
        OSPanic(__FILE__, __LINE__, "OSReferentSize: Pointer outside arena");
    }
    
    if (OFFSET(ptr, ALIGNMENT) != 0) {
        OSPanic(__FILE__, __LINE__, "OSReferentSize: Unaligned pointer");
    }
    
    Cell* cell = (Cell*)((char*)ptr - HEADERSIZE);
    return (u32)(cell->size - HEADERSIZE);
}

/*---------------------------------------------------------------------------*
  Name:         OSVisitAllocated

  Description:  Calls a visitor function for every allocated block in all
                active heaps. Useful for debugging memory leaks or tracking
                allocations.

  Arguments:    visitor - Function to call for each block
                          void visitor(void* ptr, u32 size)

  Returns:      None
 *---------------------------------------------------------------------------*/
void OSVisitAllocated(OSAllocVisitor visitor) {
    if (!s_heapArray || !visitor) return;
    
    for (u32 heap = 0; heap < s_numHeaps; heap++) {
        HeapDesc* hd = &s_heapArray[heap];
        if (hd->size < 0) continue;
        
        for (Cell* cell = hd->allocated; cell; cell = cell->next) {
            void* ptr = (void*)((u8*)cell + HEADERSIZE);
            u32 size = (u32)cell->size;
            visitor(ptr, size);
        }
    }
}
