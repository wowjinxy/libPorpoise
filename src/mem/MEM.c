#include <revolution/mem.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#define MEM_HOST_BLOCK_MAGIC ((u32)0x4D454D42)
#define MEM_HOST_MIN_SPLIT 64

typedef struct MEMHostExpBlock {
    MEMiExpHeapMBlockHead sdk;
    size_t span;
    void* payload;
    u32 magic;
    BOOL used;
    struct MEMHostExpBlock* physPrev;
    struct MEMHostExpBlock* physNext;
} MEMHostExpBlock;

typedef struct MEMHostExpMeta {
    MEMHostExpBlock* first;
} MEMHostExpMeta;

typedef struct MEMHostFrmAlloc {
    void* address;
    size_t size;
    BOOL fromTail;
    struct MEMHostFrmAlloc* next;
} MEMHostFrmAlloc;

typedef struct MEMHostFrmMeta {
    MEMHostFrmAlloc* allocations;
    void* originalEnd;
} MEMHostFrmMeta;

static MEMList sHeapList;
static BOOL sHeapListInitialized;

typedef struct MEMLegacyPointerShadow {
    void* allocation;
    void* original;
    void* shadow;
    size_t copySize;
} MEMLegacyPointerShadow;

#if defined(_WIN32) && UINTPTR_MAX > UINT32_MAX
static BOOL memIsReadableHostAddress(const void* address, size_t* available)
{
    MEMORY_BASIC_INFORMATION info;
    DWORD inaccessible;
    uintptr_t regionEnd;

    if (!address ||
        VirtualQuery(address, &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT)
        return FALSE;
    inaccessible = PAGE_NOACCESS | PAGE_GUARD;
    if (info.Protect & inaccessible)
        return FALSE;
    regionEnd = (uintptr_t)info.BaseAddress + info.RegionSize;
    *available = regionEnd - (uintptr_t)address;
    return TRUE;
}

static MEMLegacyPointerShadow memBeginLegacyPointerShadow(
    u32 value, const void* heapHint)
{
    MEMLegacyPointerShadow result = { 0 };
    uintptr_t lowAddress = (uintptr_t)value;
    uintptr_t highMask = ~((uintptr_t)UINT32_MAX);
    uintptr_t candidates[3];
    uintptr_t originalAddress = 0;
    uintptr_t allocationBase;
    size_t available = 0;
    size_t index;
    SYSTEM_INFO systemInfo;

    if (!value)
        return result;
    if (memIsReadableHostAddress((const void*)lowAddress, &available))
        return result;

    candidates[0] = ((uintptr_t)&result & highMask) | lowAddress;
    candidates[1] = ((uintptr_t)heapHint & highMask) | lowAddress;
    candidates[2] = ((uintptr_t)&memBeginLegacyPointerShadow & highMask) |
                    lowAddress;
    for (index = 0; index < sizeof(candidates) / sizeof(candidates[0]); index++) {
        if (memIsReadableHostAddress((const void*)candidates[index], &available)) {
            originalAddress = candidates[index];
            break;
        }
    }
    if (!originalAddress)
        return result;

    GetSystemInfo(&systemInfo);
    allocationBase =
        lowAddress & ~((uintptr_t)systemInfo.dwAllocationGranularity - 1);
    result.allocation = VirtualAlloc((void*)allocationBase,
                                     systemInfo.dwAllocationGranularity,
                                     MEM_RESERVE | MEM_COMMIT,
                                     PAGE_READWRITE);
    if (result.allocation != (void*)allocationBase) {
        if (result.allocation)
            VirtualFree(result.allocation, 0, MEM_RELEASE);
        memset(&result, 0, sizeof(result));
        return result;
    }

    result.original = (void*)originalAddress;
    result.shadow = (void*)lowAddress;
    result.copySize = available < 256 ? available : 256;
    memcpy(result.shadow, result.original, result.copySize);
    return result;
}

static void memEndLegacyPointerShadow(MEMLegacyPointerShadow* shadow)
{
    if (!shadow || !shadow->allocation)
        return;
    memcpy(shadow->original, shadow->shadow, shadow->copySize);
    VirtualFree(shadow->allocation, 0, MEM_RELEASE);
    memset(shadow, 0, sizeof(*shadow));
}
#else
static MEMLegacyPointerShadow memBeginLegacyPointerShadow(
    u32 value, const void* heapHint)
{
    MEMLegacyPointerShadow result = { 0 };
    (void)value;
    (void)heapHint;
    return result;
}

static void memEndLegacyPointerShadow(MEMLegacyPointerShadow* shadow)
{
    (void)shadow;
}
#endif

static uintptr_t memAlignUp(uintptr_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(uintptr_t)(alignment - 1);
}

static uintptr_t memAlignDown(uintptr_t value, size_t alignment)
{
    return value & ~(uintptr_t)(alignment - 1);
}

static size_t memNormalizeAlignment(int alignment, BOOL* fromTail)
{
    size_t result;

    *fromTail = alignment < 0;
    if (alignment == 0)
        alignment = MEM_HEAP_DEFAULT_ALIGNMENT;
    if (alignment < 0)
        alignment = -alignment;
    result = (size_t)alignment;
    if (result < sizeof(void*))
        result = sizeof(void*);
    if ((result & (result - 1)) != 0)
        return 0;
    return result;
}

static u8 memOptions(MEMHeapHandle heap)
{
    return heap ? (u8)heap->attribute.fields.optFlag : 0;
}

static void memFillAllocation(MEMHeapHandle heap, void* memory, size_t size)
{
    if (!memory || !size)
        return;
    if (memOptions(heap) & MEM_HEAP_OPT_0_CLEAR)
        memset(memory, 0, size);
    else if (memOptions(heap) & MEM_HEAP_OPT_DEBUG_FILL)
        memset(memory, 0xCD, size);
}

static void memFillFree(MEMHeapHandle heap, void* memory, size_t size)
{
    if (memory && size && (memOptions(heap) & MEM_HEAP_OPT_DEBUG_FILL))
        memset(memory, 0xDD, size);
}

static void memEnsureHeapList(void)
{
    if (!sHeapListInitialized) {
        MEMInitList(&sHeapList, (u16)offsetof(MEMiHeapHead, link));
        sHeapListInitialized = TRUE;
    }
}

static void memRegisterHeap(MEMHeapHandle heap)
{
    memEnsureHeapList();
    MEMInitList(&heap->childList, (u16)offsetof(MEMiHeapHead, link));
    MEMAppendListObject(&sHeapList, heap);
}

static void memUnregisterHeap(MEMHeapHandle heap)
{
    if (sHeapListInitialized)
        MEMRemoveListObject(&sHeapList, heap);
}

static BOOL memInitializeHeap(void* startAddress, u32 size, size_t headerSize,
                              u32 signature, u16 optFlag,
                              MEMHeapHandle* resultHeap, void** resultStart,
                              void** resultEnd)
{
    uintptr_t sourceStart;
    uintptr_t sourceEnd;
    uintptr_t heapAddress;
    uintptr_t usableStart;
    uintptr_t usableEnd;
    MEMHeapHandle heap;

    if (!startAddress || size == 0)
        return FALSE;
    sourceStart = (uintptr_t)startAddress;
    sourceEnd = sourceStart + (size_t)size;
    if (sourceEnd < sourceStart)
        return FALSE;

    heapAddress = memAlignUp(sourceStart, sizeof(void*));
    usableStart = memAlignUp(heapAddress + headerSize, sizeof(void*));
    usableEnd = memAlignDown(sourceEnd, sizeof(void*));
    if (usableStart >= usableEnd)
        return FALSE;

    heap = (MEMHeapHandle)heapAddress;
    memset(heap, 0, headerSize);
    heap->signature = signature;
    heap->heapStart = (void*)usableStart;
    heap->heapEnd = (void*)usableEnd;
    heap->attribute.val = 0;
    heap->attribute.fields.optFlag = (u8)optFlag;
    memRegisterHeap(heap);

    *resultHeap = heap;
    *resultStart = (void*)usableStart;
    *resultEnd = (void*)usableEnd;
    return TRUE;
}

void MEMInitList(MEMList* list, u16 offset)
{
    if (!list)
        return;
    list->headObject = NULL;
    list->tailObject = NULL;
    list->numObjects = 0;
    list->offset = offset;
}

static MEMLink* memGetLink(MEMList* list, void* object)
{
    return object ? (MEMLink*)((u8*)object + list->offset) : NULL;
}

void MEMAppendListObject(MEMList* list, void* object)
{
    MEMLink* link;
    MEMLink* tail;

    if (!list || !object)
        return;
    link = memGetLink(list, object);
    link->prevObject = list->tailObject;
    link->nextObject = NULL;
    tail = memGetLink(list, list->tailObject);
    if (tail)
        tail->nextObject = object;
    else
        list->headObject = object;
    list->tailObject = object;
    list->numObjects++;
}

void MEMPrependListObject(MEMList* list, void* object)
{
    MEMLink* link;
    MEMLink* head;

    if (!list || !object)
        return;
    link = memGetLink(list, object);
    link->prevObject = NULL;
    link->nextObject = list->headObject;
    head = memGetLink(list, list->headObject);
    if (head)
        head->prevObject = object;
    else
        list->tailObject = object;
    list->headObject = object;
    list->numObjects++;
}

void MEMInsertListObject(MEMList* list, void* target, void* object)
{
    MEMLink* targetLink;
    MEMLink* objectLink;
    MEMLink* previousLink;

    if (!list || !object)
        return;
    if (!target) {
        MEMAppendListObject(list, object);
        return;
    }
    targetLink = memGetLink(list, target);
    objectLink = memGetLink(list, object);
    objectLink->prevObject = targetLink->prevObject;
    objectLink->nextObject = target;
    previousLink = memGetLink(list, targetLink->prevObject);
    if (previousLink)
        previousLink->nextObject = object;
    else
        list->headObject = object;
    targetLink->prevObject = object;
    list->numObjects++;
}

void MEMRemoveListObject(MEMList* list, void* object)
{
    MEMLink* link;
    MEMLink* previous;
    MEMLink* next;

    if (!list || !object || list->numObjects == 0)
        return;
    link = memGetLink(list, object);
    previous = memGetLink(list, link->prevObject);
    next = memGetLink(list, link->nextObject);
    if (previous)
        previous->nextObject = link->nextObject;
    else
        list->headObject = link->nextObject;
    if (next)
        next->prevObject = link->prevObject;
    else
        list->tailObject = link->prevObject;
    link->prevObject = NULL;
    link->nextObject = NULL;
    list->numObjects--;
}

void* MEMGetNextListObject(MEMList* list, void* object)
{
    MEMLink* link;
    if (!list)
        return NULL;
    if (!object)
        return list->headObject;
    link = memGetLink(list, object);
    return link->nextObject;
}

void* MEMGetPrevListObject(MEMList* list, void* object)
{
    MEMLink* link;
    if (!list)
        return NULL;
    if (!object)
        return list->tailObject;
    link = memGetLink(list, object);
    return link->prevObject;
}

void* MEMGetNthListObject(MEMList* list, u16 index)
{
    void* object;
    if (!list || index >= list->numObjects)
        return NULL;
    object = list->headObject;
    while (object && index--)
        object = MEMGetNextListObject(list, object);
    return object;
}

MEMHeapHandle MEMFindContainHeap(const void* memBlock)
{
    MEMHeapHandle heap;
    MEMHeapHandle best = NULL;
    size_t bestSize = (size_t)-1;

    if (!memBlock || !sHeapListInitialized)
        return NULL;
    heap = (MEMHeapHandle)sHeapList.headObject;
    while (heap) {
        if ((const u8*)memBlock >= (const u8*)heap &&
            (const u8*)memBlock < (const u8*)heap->heapEnd) {
            size_t size = (size_t)((u8*)heap->heapEnd - (u8*)heap);
            if (size < bestSize) {
                best = heap;
                bestSize = size;
            }
        }
        heap = (MEMHeapHandle)MEMGetNextListObject(&sHeapList, heap);
    }
    return best;
}

MEMHeapHandle MEMFindParentHeap(MEMHeapHandle child)
{
    MEMHeapHandle heap;
    MEMHeapHandle best = NULL;
    size_t bestSize = (size_t)-1;

    if (!child || !sHeapListInitialized)
        return NULL;
    heap = (MEMHeapHandle)sHeapList.headObject;
    while (heap) {
        if (heap != child && (u8*)child >= (u8*)heap &&
            (u8*)child->heapEnd <= (u8*)heap->heapEnd) {
            size_t size = (size_t)((u8*)heap->heapEnd - (u8*)heap);
            if (size < bestSize) {
                best = heap;
                bestSize = size;
            }
        }
        heap = (MEMHeapHandle)MEMGetNextListObject(&sHeapList, heap);
    }
    return best;
}

static MEMiExpHeapHead* memExpHead(MEMHeapHandle heap)
{
    return (MEMiExpHeapHead*)((u8*)heap + sizeof(MEMiHeapHead));
}

static MEMHostExpMeta* memExpMeta(MEMHeapHandle heap)
{
    return (MEMHostExpMeta*)((u8*)memExpHead(heap) + sizeof(MEMiExpHeapHead));
}

static void memExpClassAdd(MEMHeapHandle heap, MEMHostExpBlock* block)
{
    MEMiExpMBlockList* list =
        block->used ? &memExpHead(heap)->mbUsedList : &memExpHead(heap)->mbFreeList;
    block->sdk.pMBHeadPrev = list->tail;
    block->sdk.pMBHeadNext = NULL;
    if (list->tail)
        list->tail->pMBHeadNext = &block->sdk;
    else
        list->head = &block->sdk;
    list->tail = &block->sdk;
}

static void memExpClassRemove(MEMHeapHandle heap, MEMHostExpBlock* block)
{
    MEMiExpMBlockList* list =
        block->used ? &memExpHead(heap)->mbUsedList : &memExpHead(heap)->mbFreeList;
    if (block->sdk.pMBHeadPrev)
        block->sdk.pMBHeadPrev->pMBHeadNext = block->sdk.pMBHeadNext;
    else
        list->head = block->sdk.pMBHeadNext;
    if (block->sdk.pMBHeadNext)
        block->sdk.pMBHeadNext->pMBHeadPrev = block->sdk.pMBHeadPrev;
    else
        list->tail = block->sdk.pMBHeadPrev;
    block->sdk.pMBHeadPrev = NULL;
    block->sdk.pMBHeadNext = NULL;
}

static void memExpInitializeBlock(MEMHostExpBlock* block, size_t span, BOOL used)
{
    memset(block, 0, sizeof(*block));
    block->span = span;
    block->magic = MEM_HOST_BLOCK_MAGIC;
    block->used = used;
    block->sdk.blockSize =
        span > sizeof(*block) ? (u32)(span - sizeof(*block)) : 0;
}

static void memExpInsertPhysicalAfter(MEMHostExpBlock* block,
                                      MEMHostExpBlock* inserted)
{
    inserted->physPrev = block;
    inserted->physNext = block->physNext;
    if (block->physNext)
        block->physNext->physPrev = inserted;
    block->physNext = inserted;
}

static MEMHostExpBlock* memExpBlockFromMemory(const void* memory)
{
    MEMHostExpBlock* block;
    if (!memory)
        return NULL;
    block = *((MEMHostExpBlock* const*)((const u8*)memory - sizeof(block)));
    if (!block || block->magic != MEM_HOST_BLOCK_MAGIC ||
        !block->used || block->payload != memory)
        return NULL;
    return block;
}

static size_t memExpFrontRequired(MEMHostExpBlock* block, size_t size,
                                  size_t alignment, void** payload)
{
    uintptr_t base = (uintptr_t)block;
    uintptr_t data = memAlignUp(base + sizeof(*block) + sizeof(block), alignment);
    uintptr_t end = memAlignUp(data + size, sizeof(void*));
    if (end < data || end > base + block->span)
        return 0;
    *payload = (void*)data;
    return (size_t)(end - base);
}

static size_t memExpRearRequired(MEMHostExpBlock* block, size_t size,
                                 size_t alignment, void** payload,
                                 uintptr_t* usedAddress)
{
    uintptr_t base = (uintptr_t)block;
    uintptr_t end = base + block->span;
    uintptr_t data;
    uintptr_t used;

    if (size > block->span)
        return 0;
    data = memAlignDown(end - size, alignment);
    if (data < base + sizeof(*block) + sizeof(block))
        return 0;
    used = memAlignDown(data - sizeof(block) - sizeof(*block), sizeof(void*));
    if (used < base)
        return 0;
    *payload = (void*)data;
    *usedAddress = used;
    return (size_t)(end - used);
}

MEMHeapHandle MEMCreateExpHeapEx(void* startAddress, u32 size, u16 optFlag)
{
    MEMHeapHandle heap;
    void* usableStart;
    void* usableEnd;
    size_t headerSize =
        sizeof(MEMiHeapHead) + sizeof(MEMiExpHeapHead) + sizeof(MEMHostExpMeta);
    MEMHostExpBlock* block;

    if (!memInitializeHeap(startAddress, size, headerSize,
                           MEMi_EXPHEAP_SIGNATURE, optFlag,
                           &heap, &usableStart, &usableEnd))
        return NULL;
    if ((size_t)((u8*)usableEnd - (u8*)usableStart) <
        sizeof(MEMHostExpBlock) + sizeof(void*)) {
        memUnregisterHeap(heap);
        heap->signature = 0;
        return NULL;
    }

    block = (MEMHostExpBlock*)usableStart;
    memExpInitializeBlock(block, (size_t)((u8*)usableEnd - (u8*)usableStart),
                          FALSE);
    memExpMeta(heap)->first = block;
    memExpClassAdd(heap, block);
    return heap;
}

void* MEMDestroyExpHeap(MEMHeapHandle heap)
{
    if (!MEMIsExpHeap(heap))
        return NULL;
    memUnregisterHeap(heap);
    heap->signature = 0;
    return heap;
}

void* MEMAllocFromExpHeapEx(MEMHeapHandle heap, u32 size, int alignment)
{
    MEMHostExpBlock* block;
    MEMHostExpBlock* selected = NULL;
    size_t selectedRequired = 0;
    size_t normalized;
    BOOL fromTail;
    void* selectedPayload = NULL;
    uintptr_t selectedUsedAddress = 0;
    BOOL nearMode;

    if (!MEMIsExpHeap(heap) || size == 0)
        return NULL;
    normalized = memNormalizeAlignment(alignment, &fromTail);
    if (!normalized)
        return NULL;
    nearMode = memExpHead(heap)->feature.fields.allocMode ==
               MEM_EXPHEAP_ALLOC_MODE_NEAR;

    block = memExpMeta(heap)->first;
    while (block) {
        if (!block->used) {
            void* payload = NULL;
            uintptr_t usedAddress = 0;
            size_t required = fromTail
                ? memExpRearRequired(block, size, normalized, &payload, &usedAddress)
                : memExpFrontRequired(block, size, normalized, &payload);
            if (required && (!selected || !nearMode ||
                             block->span - required <
                                 selected->span - selectedRequired)) {
                selected = block;
                selectedRequired = required;
                selectedPayload = payload;
                selectedUsedAddress = usedAddress;
                if (!nearMode)
                    break;
            }
        }
        block = block->physNext;
    }
    if (!selected)
        return NULL;

    memExpClassRemove(heap, selected);
    if (!fromTail) {
        size_t remainder = selected->span - selectedRequired;
        if (remainder >= sizeof(MEMHostExpBlock) + MEM_HOST_MIN_SPLIT) {
            MEMHostExpBlock* freeBlock =
                (MEMHostExpBlock*)((u8*)selected + selectedRequired);
            memExpInitializeBlock(freeBlock, remainder, FALSE);
            memExpInsertPhysicalAfter(selected, freeBlock);
            selected->span = selectedRequired;
            memExpClassAdd(heap, freeBlock);
        }
    } else {
        size_t prefix = (size_t)(selectedUsedAddress - (uintptr_t)selected);
        if (prefix >= sizeof(MEMHostExpBlock) + MEM_HOST_MIN_SPLIT) {
            MEMHostExpBlock* usedBlock =
                (MEMHostExpBlock*)selectedUsedAddress;
            MEMHostExpBlock* oldNext = selected->physNext;
            memExpInitializeBlock(usedBlock, selectedRequired, TRUE);
            selected->span = prefix;
            selected->sdk.blockSize =
                (u32)(prefix > sizeof(*selected) ? prefix - sizeof(*selected) : 0);
            usedBlock->physPrev = selected;
            usedBlock->physNext = oldNext;
            selected->physNext = usedBlock;
            if (oldNext)
                oldNext->physPrev = usedBlock;
            memExpClassAdd(heap, selected);
            selected = usedBlock;
        }
    }

    selected->used = TRUE;
    selected->payload = selectedPayload;
    selected->sdk.signature = (u16)0x5544;
    selected->sdk.blockSize = size;
    selected->sdk.attribute.val = 0;
    selected->sdk.attribute.fields.allocDir =
        fromTail ? MEM_EXPHEAP_ALLOC_DIR_REAR : MEM_EXPHEAP_ALLOC_DIR_FRONT;
    selected->sdk.attribute.fields.groupID =
        (u16)(memExpHead(heap)->groupID & 0xff);
    *((MEMHostExpBlock**)((u8*)selectedPayload - sizeof(selected))) = selected;
    memExpClassAdd(heap, selected);
    memFillAllocation(heap, selectedPayload, size);
    return selectedPayload;
}

static MEMHostExpBlock* memExpCoalesce(MEMHeapHandle heap,
                                       MEMHostExpBlock* block)
{
    MEMHostExpBlock* previous = block->physPrev;
    MEMHostExpBlock* next = block->physNext;

    if (previous && !previous->used) {
        memExpClassRemove(heap, previous);
        previous->span += block->span;
        previous->physNext = block->physNext;
        if (block->physNext)
            block->physNext->physPrev = previous;
        block->magic = 0;
        block = previous;
    }
    next = block->physNext;
    if (next && !next->used) {
        memExpClassRemove(heap, next);
        block->span += next->span;
        block->physNext = next->physNext;
        if (next->physNext)
            next->physNext->physPrev = block;
        next->magic = 0;
    }
    block->sdk.blockSize =
        (u32)(block->span > sizeof(*block) ? block->span - sizeof(*block) : 0);
    return block;
}

void MEMFreeToExpHeap(MEMHeapHandle heap, void* memBlock)
{
    MEMHostExpBlock* block = memExpBlockFromMemory(memBlock);
    if (!MEMIsExpHeap(heap) || !block)
        return;
    memExpClassRemove(heap, block);
    memFillFree(heap, memBlock, block->sdk.blockSize);
    block->used = FALSE;
    block->payload = NULL;
    block->sdk.signature = 0;
    block = memExpCoalesce(heap, block);
    memExpClassAdd(heap, block);
}

u32 MEMResizeForMBlockExpHeap(MEMHeapHandle heap, void* memBlock, u32 size)
{
    MEMHostExpBlock* block = memExpBlockFromMemory(memBlock);
    uintptr_t blockEnd;
    uintptr_t requestedEnd;
    uintptr_t splitAddress;
    size_t oldSize;

    if (!MEMIsExpHeap(heap) || !block || size == 0)
        return 0;
    oldSize = block->sdk.blockSize;
    blockEnd = (uintptr_t)block + block->span;
    requestedEnd = (uintptr_t)memBlock + size;
    if (requestedEnd > blockEnd && block->physNext && !block->physNext->used) {
        MEMHostExpBlock* next = block->physNext;
        memExpClassRemove(heap, next);
        block->span += next->span;
        block->physNext = next->physNext;
        if (next->physNext)
            next->physNext->physPrev = block;
        next->magic = 0;
        blockEnd = (uintptr_t)block + block->span;
    }
    if (requestedEnd > blockEnd)
        return 0;

    splitAddress = memAlignUp(requestedEnd, sizeof(void*));
    if (blockEnd - splitAddress >=
        sizeof(MEMHostExpBlock) + MEM_HOST_MIN_SPLIT) {
        MEMHostExpBlock* freeBlock = (MEMHostExpBlock*)splitAddress;
        size_t oldSpan = block->span;
        memExpInitializeBlock(freeBlock,
                              oldSpan - (size_t)(splitAddress - (uintptr_t)block),
                              FALSE);
        memExpInsertPhysicalAfter(block, freeBlock);
        block->span = (size_t)(splitAddress - (uintptr_t)block);
        freeBlock = memExpCoalesce(heap, freeBlock);
        memExpClassAdd(heap, freeBlock);
    }
    block->sdk.blockSize = size;
    if (size > oldSize)
        memFillAllocation(heap, (u8*)memBlock + oldSize, size - oldSize);
    return size;
}

u32 MEMGetTotalFreeSizeForExpHeap(MEMHeapHandle heap)
{
    MEMHostExpBlock* block;
    size_t total = 0;
    if (!MEMIsExpHeap(heap))
        return 0;
    for (block = memExpMeta(heap)->first; block; block = block->physNext) {
        if (!block->used && block->span > sizeof(*block) + sizeof(block))
            total += block->span - sizeof(*block) - sizeof(block);
    }
    return (u32)total;
}

u32 MEMGetAllocatableSizeForExpHeapEx(MEMHeapHandle heap, int alignment)
{
    MEMHostExpBlock* block;
    size_t normalized;
    size_t largest = 0;
    BOOL fromTail;

    if (!MEMIsExpHeap(heap))
        return 0;
    normalized = memNormalizeAlignment(alignment, &fromTail);
    if (!normalized)
        return 0;
    for (block = memExpMeta(heap)->first; block; block = block->physNext) {
        if (!block->used) {
            uintptr_t base = (uintptr_t)block;
            uintptr_t end = base + block->span;
            size_t capacity = 0;
            if (fromTail) {
                uintptr_t minimum =
                    memAlignUp(base + sizeof(*block) + sizeof(block), normalized);
                if (end > minimum)
                    capacity = (size_t)(end - minimum);
            } else {
                uintptr_t payload =
                    memAlignUp(base + sizeof(*block) + sizeof(block), normalized);
                if (end > payload)
                    capacity = (size_t)(end - payload);
            }
            if (capacity > largest)
                largest = capacity;
        }
    }
    return (u32)largest;
}

BOOL MEMiIsEmptyExpHeap(MEMHeapHandle heap)
{
    return MEMIsExpHeap(heap) && memExpHead(heap)->mbUsedList.head == NULL;
}

u16 MEMSetAllocModeForExpHeap(MEMHeapHandle heap, u16 mode)
{
    u16 old;
    if (!MEMIsExpHeap(heap))
        return MEM_EXPHEAP_ALLOC_MODE_FIRST;
    old = memExpHead(heap)->feature.fields.allocMode;
    memExpHead(heap)->feature.fields.allocMode =
        mode == MEM_EXPHEAP_ALLOC_MODE_NEAR;
    return old;
}

u16 MEMGetAllocModeForExpHeap(MEMHeapHandle heap)
{
    return MEMIsExpHeap(heap) ? memExpHead(heap)->feature.fields.allocMode : 0;
}

BOOL MEMUseMarginOfAlignmentForExpHeap(MEMHeapHandle heap, BOOL reuse)
{
    BOOL old;
    if (!MEMIsExpHeap(heap))
        return FALSE;
    old = memExpHead(heap)->feature.fields.useMarginOfAlign;
    memExpHead(heap)->feature.fields.useMarginOfAlign = reuse != FALSE;
    return old;
}

u16 MEMSetGroupIDForExpHeap(MEMHeapHandle heap, u16 groupID)
{
    u16 old;
    if (!MEMIsExpHeap(heap))
        return 0;
    old = memExpHead(heap)->groupID;
    memExpHead(heap)->groupID = (u16)(groupID & 0xff);
    return old;
}

u16 MEMGetGroupIDForExpHeap(MEMHeapHandle heap)
{
    return MEMIsExpHeap(heap) ? memExpHead(heap)->groupID : 0;
}

void MEMVisitAllocatedForExpHeap(MEMHeapHandle heap, MEMHeapVisitor visitor,
                                 u32 userParam)
{
    MEMHostExpBlock* block;
    void** allocations;
    size_t allocationCount = 0;
    size_t index = 0;
    MEMLegacyPointerShadow shadow;

    if (!MEMIsExpHeap(heap) || !visitor)
        return;
    for (block = memExpMeta(heap)->first; block; block = block->physNext) {
        if (block->used)
            allocationCount++;
    }
    allocations = allocationCount
        ? (void**)malloc(allocationCount * sizeof(*allocations))
        : NULL;
    if (allocationCount && !allocations)
        return;
    for (block = memExpMeta(heap)->first; block; block = block->physNext) {
        if (block->used)
            allocations[index++] = block->payload;
    }

    shadow = memBeginLegacyPointerShadow(userParam, heap);
    for (index = 0; index < allocationCount; index++) {
        if (MEMCheckForMBlockExpHeap(allocations[index], heap, 0))
            visitor(allocations[index], heap, userParam);
    }
    memEndLegacyPointerShadow(&shadow);
    free(allocations);
}

u32 MEMGetSizeForMBlockExpHeap(const void* memBlock)
{
    MEMHostExpBlock* block = memExpBlockFromMemory(memBlock);
    return block ? block->sdk.blockSize : 0;
}

u16 MEMGetGroupIDForMBlockExpHeap(const void* memBlock)
{
    MEMHostExpBlock* block = memExpBlockFromMemory(memBlock);
    return block ? block->sdk.attribute.fields.groupID : 0;
}

u16 MEMGetAllocDirForMBlockExpHeap(const void* memBlock)
{
    MEMHostExpBlock* block = memExpBlockFromMemory(memBlock);
    return block ? block->sdk.attribute.fields.allocDir :
                   MEM_EXPHEAP_ALLOC_DIR_FRONT;
}

u32 MEMAdjustExpHeap(MEMHeapHandle heap)
{
    MEMHostExpBlock* block;
    if (!MEMIsExpHeap(heap))
        return 0;
    block = memExpMeta(heap)->first;
    if (!block)
        return 0;
    while (block->physNext)
        block = block->physNext;
    if (!block->used) {
        memExpClassRemove(heap, block);
        heap->heapEnd = block;
        if (block->physPrev)
            block->physPrev->physNext = NULL;
        else
            memExpMeta(heap)->first = NULL;
        block->magic = 0;
    }
    return (u32)((u8*)heap->heapEnd - (u8*)heap);
}

BOOL MEMCheckExpHeap(MEMHeapHandle heap, u32 optFlag)
{
    MEMHostExpBlock* block;
    uintptr_t expected;
    (void)optFlag;
    if (!MEMIsExpHeap(heap))
        return FALSE;
    expected = (uintptr_t)heap->heapStart;
    for (block = memExpMeta(heap)->first; block; block = block->physNext) {
        if ((uintptr_t)block != expected || block->magic != MEM_HOST_BLOCK_MAGIC ||
            block->span < sizeof(*block))
            return FALSE;
        expected += block->span;
    }
    return expected == (uintptr_t)heap->heapEnd;
}

BOOL MEMCheckForMBlockExpHeap(const void* memBlock, MEMHeapHandle heap,
                              u32 optFlag)
{
    MEMHostExpBlock* block = memExpBlockFromMemory(memBlock);
    MEMHostExpBlock* current;
    (void)optFlag;
    if (!MEMIsExpHeap(heap) || !block)
        return FALSE;
    for (current = memExpMeta(heap)->first; current; current = current->physNext) {
        if (current == block)
            return TRUE;
    }
    return FALSE;
}

static MEMiFrmHeapHead* memFrmHead(MEMHeapHandle heap)
{
    return (MEMiFrmHeapHead*)((u8*)heap + sizeof(MEMiHeapHead));
}

static MEMHostFrmMeta* memFrmMeta(MEMHeapHandle heap)
{
    return (MEMHostFrmMeta*)((u8*)memFrmHead(heap) + sizeof(MEMiFrmHeapHead));
}

static void memFrmRemoveAllocations(MEMHeapHandle heap, int mode)
{
    MEMHostFrmAlloc** link = &memFrmMeta(heap)->allocations;
    while (*link) {
        MEMHostFrmAlloc* allocation = *link;
        BOOL remove = (allocation->fromTail && (mode & MEM_FRMHEAP_FREE_TAIL)) ||
                      (!allocation->fromTail && (mode & MEM_FRMHEAP_FREE_HEAD));
        if (remove) {
            *link = allocation->next;
            free(allocation);
        } else {
            link = &allocation->next;
        }
    }
}

static void memFrmDiscardStates(MEMHeapHandle heap)
{
    MEMiFrmHeapState* state = memFrmHead(heap)->pState;
    while (state) {
        MEMiFrmHeapState* previous = state->pPrevState;
        free(state);
        state = previous;
    }
    memFrmHead(heap)->pState = NULL;
}

void* MEMiGetFreeStartForFrmHeap(MEMHeapHandle heap)
{
    return MEMIsFrmHeap(heap) ? memFrmHead(heap)->headAllocator : NULL;
}

void* MEMiGetFreeEndForFrmHeap(MEMHeapHandle heap)
{
    return MEMIsFrmHeap(heap) ? memFrmHead(heap)->tailAllocator : NULL;
}

MEMHeapHandle MEMCreateFrmHeapEx(void* startAddress, u32 size, u16 optFlag)
{
    MEMHeapHandle heap;
    void* usableStart;
    void* usableEnd;
    size_t headerSize =
        sizeof(MEMiHeapHead) + sizeof(MEMiFrmHeapHead) + sizeof(MEMHostFrmMeta);

    if (!memInitializeHeap(startAddress, size, headerSize,
                           MEMi_FRMHEAP_SIGNATURE, optFlag,
                           &heap, &usableStart, &usableEnd))
        return NULL;
    memFrmHead(heap)->headAllocator = usableStart;
    memFrmHead(heap)->tailAllocator = usableEnd;
    memFrmHead(heap)->pState = NULL;
    memFrmMeta(heap)->allocations = NULL;
    memFrmMeta(heap)->originalEnd = usableEnd;
    return heap;
}

void* MEMDestroyFrmHeap(MEMHeapHandle heap)
{
    if (!MEMIsFrmHeap(heap))
        return NULL;
    memFrmRemoveAllocations(heap, MEM_FRMHEAP_FREE_ALL);
    memFrmDiscardStates(heap);
    memUnregisterHeap(heap);
    heap->signature = 0;
    return heap;
}

void* MEMAllocFromFrmHeapEx(MEMHeapHandle heap, u32 size, int alignment)
{
    MEMiFrmHeapHead* head;
    MEMHostFrmAlloc* record;
    size_t normalized;
    BOOL fromTail;
    uintptr_t address;
    uintptr_t end;

    if (!MEMIsFrmHeap(heap) || size == 0)
        return NULL;
    normalized = memNormalizeAlignment(alignment, &fromTail);
    if (!normalized)
        return NULL;
    head = memFrmHead(heap);
    if (fromTail) {
        address = memAlignDown((uintptr_t)head->tailAllocator - size, normalized);
        end = (uintptr_t)head->tailAllocator;
        if (address < (uintptr_t)head->headAllocator)
            return NULL;
        head->tailAllocator = (void*)address;
    } else {
        address = memAlignUp((uintptr_t)head->headAllocator, normalized);
        end = address + size;
        if (end < address || end > (uintptr_t)head->tailAllocator)
            return NULL;
        head->headAllocator = (void*)end;
    }

    record = (MEMHostFrmAlloc*)malloc(sizeof(*record));
    if (!record) {
        if (fromTail)
            head->tailAllocator = (void*)end;
        else
            head->headAllocator = (void*)address;
        return NULL;
    }
    record->address = (void*)address;
    record->size = size;
    record->fromTail = fromTail;
    record->next = memFrmMeta(heap)->allocations;
    memFrmMeta(heap)->allocations = record;
    memFillAllocation(heap, (void*)address, size);
    return (void*)address;
}

void MEMFreeToFrmHeap(MEMHeapHandle heap, int mode)
{
    if (!MEMIsFrmHeap(heap))
        return;
    if (mode & MEM_FRMHEAP_FREE_HEAD)
        memFrmHead(heap)->headAllocator = heap->heapStart;
    if (mode & MEM_FRMHEAP_FREE_TAIL)
        memFrmHead(heap)->tailAllocator = heap->heapEnd;
    memFrmRemoveAllocations(heap, mode);
    memFrmDiscardStates(heap);
}

u32 MEMGetAllocatableSizeForFrmHeapEx(MEMHeapHandle heap, int alignment)
{
    size_t normalized;
    BOOL fromTail;
    uintptr_t start;
    uintptr_t end;

    if (!MEMIsFrmHeap(heap))
        return 0;
    normalized = memNormalizeAlignment(alignment, &fromTail);
    if (!normalized)
        return 0;
    start = (uintptr_t)memFrmHead(heap)->headAllocator;
    end = (uintptr_t)memFrmHead(heap)->tailAllocator;
    if (fromTail)
        end = memAlignDown(end, normalized);
    else
        start = memAlignUp(start, normalized);
    return end > start ? (u32)(end - start) : 0;
}

BOOL MEMRecordStateForFrmHeap(MEMHeapHandle heap, u32 tagName)
{
    MEMiFrmHeapState* state;
    if (!MEMIsFrmHeap(heap))
        return FALSE;
    state = (MEMiFrmHeapState*)malloc(sizeof(*state));
    if (!state)
        return FALSE;
    state->tagName = tagName;
    state->headAllocator = memFrmHead(heap)->headAllocator;
    state->tailAllocator = memFrmHead(heap)->tailAllocator;
    state->pPrevState = memFrmHead(heap)->pState;
    memFrmHead(heap)->pState = state;
    return TRUE;
}

BOOL MEMFreeByStateToFrmHeap(MEMHeapHandle heap, u32 tagName)
{
    MEMiFrmHeapState* state;
    MEMiFrmHeapState* current;
    MEMHostFrmAlloc** allocationLink;

    if (!MEMIsFrmHeap(heap))
        return FALSE;
    state = memFrmHead(heap)->pState;
    while (state && tagName != 0 && state->tagName != tagName)
        state = state->pPrevState;
    if (!state)
        return FALSE;
    memFrmHead(heap)->headAllocator = state->headAllocator;
    memFrmHead(heap)->tailAllocator = state->tailAllocator;

    allocationLink = &memFrmMeta(heap)->allocations;
    while (*allocationLink) {
        MEMHostFrmAlloc* allocation = *allocationLink;
        uintptr_t address = (uintptr_t)allocation->address;
        if ((!allocation->fromTail &&
             address >= (uintptr_t)state->headAllocator) ||
            (allocation->fromTail &&
             address < (uintptr_t)state->tailAllocator)) {
            *allocationLink = allocation->next;
            free(allocation);
        } else {
            allocationLink = &allocation->next;
        }
    }

    current = memFrmHead(heap)->pState;
    while (current) {
        MEMiFrmHeapState* previous = current->pPrevState;
        free(current);
        if (current == state) {
            memFrmHead(heap)->pState = previous;
            break;
        }
        current = previous;
    }
    return TRUE;
}

u32 MEMAdjustFrmHeap(MEMHeapHandle heap)
{
    if (!MEMIsFrmHeap(heap))
        return 0;
    if (memFrmHead(heap)->tailAllocator != memFrmMeta(heap)->originalEnd)
        return 0;
    heap->heapEnd = memFrmHead(heap)->headAllocator;
    memFrmHead(heap)->tailAllocator = heap->heapEnd;
    memFrmMeta(heap)->originalEnd = heap->heapEnd;
    return (u32)((u8*)heap->heapEnd - (u8*)heap);
}

u32 MEMResizeForMBlockFrmHeap(MEMHeapHandle heap, void* memBlock, u32 newSize)
{
    MEMHostFrmAlloc* allocation;
    uintptr_t newEnd;
    if (!MEMIsFrmHeap(heap) || !memBlock || newSize == 0)
        return 0;
    allocation = memFrmMeta(heap)->allocations;
    while (allocation && allocation->address != memBlock)
        allocation = allocation->next;
    if (!allocation || allocation->fromTail)
        return 0;
    if ((u8*)allocation->address + allocation->size !=
        (u8*)memFrmHead(heap)->headAllocator)
        return 0;
    newEnd = (uintptr_t)allocation->address + newSize;
    if (newEnd > (uintptr_t)memFrmHead(heap)->tailAllocator)
        return 0;
    if (newSize > allocation->size)
        memFillAllocation(heap, (u8*)memBlock + allocation->size,
                          newSize - allocation->size);
    allocation->size = newSize;
    memFrmHead(heap)->headAllocator = (void*)newEnd;
    return newSize;
}

static MEMiUntHeapHead* memUnitHead(MEMHeapHandle heap)
{
    return (MEMiUntHeapHead*)((u8*)heap + sizeof(MEMiHeapHead));
}

MEMHeapHandle MEMCreateUnitHeapEx(void* startAddress, u32 heapSize,
                                 u32 memBlockSize, int alignment, u16 optFlag)
{
    MEMHeapHandle heap;
    void* usableStart;
    void* usableEnd;
    size_t normalized;
    size_t blockSize;
    size_t headerSize = sizeof(MEMiHeapHead) + sizeof(MEMiUntHeapHead);
    BOOL unusedTail;
    u8* block;
    MEMiUntHeapMBlockHead* previous = NULL;

    normalized = memNormalizeAlignment(alignment, &unusedTail);
    if (!normalized || unusedTail || memBlockSize == 0)
        return NULL;
    if (!memInitializeHeap(startAddress, heapSize, headerSize,
                           MEMi_UNTHEAP_SIGNATURE, optFlag,
                           &heap, &usableStart, &usableEnd))
        return NULL;
    usableStart = (void*)memAlignUp((uintptr_t)usableStart, normalized);
    heap->heapStart = usableStart;
    blockSize = memAlignUp(memBlockSize < sizeof(void*) ? sizeof(void*) : memBlockSize,
                           normalized);
    memUnitHead(heap)->mBlkSize = (u32)blockSize;
    memUnitHead(heap)->mbFreeList.head = NULL;
    for (block = (u8*)usableStart;
         block + blockSize <= (u8*)usableEnd;
         block += blockSize) {
        MEMiUntHeapMBlockHead* current = (MEMiUntHeapMBlockHead*)block;
        if (previous)
            previous->pMBlkHdNext = current;
        else
            memUnitHead(heap)->mbFreeList.head = current;
        previous = current;
    }
    if (previous)
        previous->pMBlkHdNext = NULL;
    if (!memUnitHead(heap)->mbFreeList.head) {
        memUnregisterHeap(heap);
        heap->signature = 0;
        return NULL;
    }
    return heap;
}

void* MEMDestroyUnitHeap(MEMHeapHandle heap)
{
    if (!MEMIsUnitHeap(heap))
        return NULL;
    memUnregisterHeap(heap);
    heap->signature = 0;
    return heap;
}

void* MEMAllocFromUnitHeap(MEMHeapHandle heap)
{
    MEMiUntHeapMBlockHead* block;
    if (!MEMIsUnitHeap(heap))
        return NULL;
    block = memUnitHead(heap)->mbFreeList.head;
    if (!block)
        return NULL;
    memUnitHead(heap)->mbFreeList.head = block->pMBlkHdNext;
    memFillAllocation(heap, block, memUnitHead(heap)->mBlkSize);
    return block;
}

void MEMFreeToUnitHeap(MEMHeapHandle heap, void* memBlock)
{
    MEMiUntHeapMBlockHead* block = (MEMiUntHeapMBlockHead*)memBlock;
    size_t blockSize;
    if (!MEMIsUnitHeap(heap) || !memBlock)
        return;
    blockSize = memUnitHead(heap)->mBlkSize;
    if ((u8*)memBlock < (u8*)heap->heapStart ||
        (u8*)memBlock + blockSize > (u8*)heap->heapEnd ||
        ((size_t)((u8*)memBlock - (u8*)heap->heapStart) % blockSize) != 0)
        return;
    memFillFree(heap, memBlock, blockSize);
    block->pMBlkHdNext = memUnitHead(heap)->mbFreeList.head;
    memUnitHead(heap)->mbFreeList.head = block;
}

u32 MEMCountFreeBlockForUnitHeap(MEMHeapHandle heap)
{
    MEMiUntHeapMBlockHead* block;
    u32 count = 0;
    if (!MEMIsUnitHeap(heap))
        return 0;
    block = memUnitHead(heap)->mbFreeList.head;
    while (block) {
        count++;
        block = block->pMBlkHdNext;
    }
    return count;
}

u32 MEMCalcHeapSizeForUnitHeap(u32 memBlockSize, u32 memBlockNum, int alignment)
{
    size_t normalized;
    size_t headerSize;
    size_t blockSize;
    BOOL unusedTail;
    normalized = memNormalizeAlignment(alignment, &unusedTail);
    if (!normalized || unusedTail || memBlockSize == 0)
        return 0;
    headerSize = memAlignUp(sizeof(MEMiHeapHead) + sizeof(MEMiUntHeapHead),
                            normalized);
    blockSize = memAlignUp(memBlockSize < sizeof(void*) ? sizeof(void*) : memBlockSize,
                           normalized);
    return (u32)(headerSize + blockSize * memBlockNum + normalized - 1);
}

static void* memAllocatorExpAlloc(MEMAllocator* allocator, u32 size)
{
    return MEMAllocFromExpHeapEx((MEMHeapHandle)allocator->pHeap, size,
                                 (int)allocator->heapParam1);
}

static void memAllocatorExpFree(MEMAllocator* allocator, void* memory)
{
    MEMFreeToExpHeap((MEMHeapHandle)allocator->pHeap, memory);
}

static void* memAllocatorFrmAlloc(MEMAllocator* allocator, u32 size)
{
    return MEMAllocFromFrmHeapEx((MEMHeapHandle)allocator->pHeap, size,
                                 (int)allocator->heapParam1);
}

static void memAllocatorFrmFree(MEMAllocator* allocator, void* memory)
{
    (void)allocator;
    (void)memory;
}

static void* memAllocatorUnitAlloc(MEMAllocator* allocator, u32 size)
{
    if (size > MEMGetMemBlockSizeForUnitHeap((MEMHeapHandle)allocator->pHeap))
        return NULL;
    return MEMAllocFromUnitHeap((MEMHeapHandle)allocator->pHeap);
}

static void memAllocatorUnitFree(MEMAllocator* allocator, void* memory)
{
    MEMFreeToUnitHeap((MEMHeapHandle)allocator->pHeap, memory);
}

static void* memAllocatorOSAlloc(MEMAllocator* allocator, u32 size)
{
    return OSAllocFromHeap((OSHeapHandle)(intptr_t)allocator->pHeap, size);
}

static void memAllocatorOSFree(MEMAllocator* allocator, void* memory)
{
    OSFreeToHeap((OSHeapHandle)(intptr_t)allocator->pHeap, memory);
}

static const MEMAllocatorFunc sExpAllocator = {
    memAllocatorExpAlloc, memAllocatorExpFree
};
static const MEMAllocatorFunc sFrmAllocator = {
    memAllocatorFrmAlloc, memAllocatorFrmFree
};
static const MEMAllocatorFunc sUnitAllocator = {
    memAllocatorUnitAlloc, memAllocatorUnitFree
};
static const MEMAllocatorFunc sOSAllocator = {
    memAllocatorOSAlloc, memAllocatorOSFree
};

void* MEMAllocFromAllocator(MEMAllocator* allocator, u32 size)
{
    if (!allocator || !allocator->pFunc || !allocator->pFunc->pfAlloc)
        return NULL;
    return allocator->pFunc->pfAlloc(allocator, size);
}

void MEMFreeToAllocator(MEMAllocator* allocator, void* memBlock)
{
    if (!allocator || !allocator->pFunc || !allocator->pFunc->pfFree || !memBlock)
        return;
    allocator->pFunc->pfFree(allocator, memBlock);
}

void MEMInitAllocatorForExpHeap(MEMAllocator* allocator, MEMHeapHandle heap,
                                int alignment)
{
    if (!allocator)
        return;
    allocator->pFunc = &sExpAllocator;
    allocator->pHeap = heap;
    allocator->heapParam1 = (u32)alignment;
    allocator->heapParam2 = 0;
}

void MEMInitAllocatorForFrmHeap(MEMAllocator* allocator, MEMHeapHandle heap,
                                int alignment)
{
    if (!allocator)
        return;
    allocator->pFunc = &sFrmAllocator;
    allocator->pHeap = heap;
    allocator->heapParam1 = (u32)alignment;
    allocator->heapParam2 = 0;
}

void MEMInitAllocatorForUnitHeap(MEMAllocator* allocator, MEMHeapHandle heap)
{
    if (!allocator)
        return;
    allocator->pFunc = &sUnitAllocator;
    allocator->pHeap = heap;
    allocator->heapParam1 = 0;
    allocator->heapParam2 = 0;
}

void MEMInitAllocatorForOSHeap(MEMAllocator* allocator, OSHeapHandle heap)
{
    if (!allocator)
        return;
    allocator->pFunc = &sOSAllocator;
    allocator->pHeap = (void*)(intptr_t)heap;
    allocator->heapParam1 = 0;
    allocator->heapParam2 = 0;
}
