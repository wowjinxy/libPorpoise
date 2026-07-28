#include <dolphin/os/OSHostAddress.h>

#ifdef LIBPORPOISE_PORT

#include <dolphin/os/OSHostMemory.h>
#include <SDL2/SDL_atomic.h>
#include <SDL2/SDL_mutex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define HOST_ADDRESS_GENERATION_BITS 14U
#define HOST_ADDRESS_GENERATION_MAX  ((1U << HOST_ADDRESS_GENERATION_BITS) - 1U)
#define HOST_ADDRESS_SLOT_MASK       (OS_HOST_ADDRESS_TOKEN_SLOT_COUNT - 1U)
#define HOST_ADDRESS_FREE_END        0xFFFFU

typedef struct OSHostAddressSlot {
	const void* pointer;
	u16 generation;
	u16 nextFree;
	BOOL inUse;
} OSHostAddressSlot;

static OSHostAddressSlot HostAddressSlots[OS_HOST_ADDRESS_TOKEN_SLOT_COUNT];
static u16 HostAddressFreeHead;
static SDL_mutex* HostAddressMutex;
static SDL_SpinLock HostAddressInitLock;

static BOOL AddressIsInRange(
	uintptr_t address,
	uintptr_t base,
	u32 size)
{
	return address >= base && address - base < size;
}

static SDL_mutex* GetHostAddressMutex(void)
{
	SDL_mutex* mutex;
	u32 index;

	SDL_AtomicLock(&HostAddressInitLock);
	if (HostAddressMutex == NULL) {
		HostAddressMutex = SDL_CreateMutex();
		if (HostAddressMutex != NULL) {
			for (index = 0; index < OS_HOST_ADDRESS_TOKEN_SLOT_COUNT; ++index) {
				HostAddressSlots[index].pointer = NULL;
				HostAddressSlots[index].generation = 1;
				HostAddressSlots[index].nextFree =
				    index + 1U < OS_HOST_ADDRESS_TOKEN_SLOT_COUNT
				        ? (u16)(index + 1U)
				        : HOST_ADDRESS_FREE_END;
				HostAddressSlots[index].inUse = FALSE;
			}
			HostAddressFreeHead = 0;
		}
	}
	mutex = HostAddressMutex;
	SDL_AtomicUnlock(&HostAddressInitLock);

	if (mutex == NULL) {
		fprintf(
		    stderr,
		    "libPorpoise: could not initialize the host-address "
		    "translator mutex.\n");
		abort();
	}
	return mutex;
}

static u32 MakeToken(u32 slot, u32 generation)
{
	return OS_HOST_ADDRESS_TOKEN_TAG |
	       (generation << OS_HOST_ADDRESS_TOKEN_SLOT_BITS) |
	       slot;
}

BOOL __OSHostIsAddressToken(u32 address)
{
	return (address & OS_HOST_ADDRESS_TOKEN_MASK) ==
	       OS_HOST_ADDRESS_TOKEN_TAG;
}

u32 __OSHostEncodeAddress(const void* pointer)
{
	const OSHostMemoryLayout* layout;
	uintptr_t address;
	SDL_mutex* mutex;
	OSHostAddressSlot* entry;
	u16 slot;
	u32 token;

	if (pointer == NULL) {
		return 0;
	}

	address = (uintptr_t)pointer;
	layout = __OSHostMemoryGetLayout();
	if (layout != NULL &&
	    (AddressIsInRange(
	         address,
	         (uintptr_t)layout->cachedBase,
	         layout->size) ||
	     AddressIsInRange(
	         address,
	         (uintptr_t)layout->uncachedBase,
	         layout->size))) {
		return (u32)address;
	}

	mutex = GetHostAddressMutex();
	SDL_LockMutex(mutex);
	if (HostAddressFreeHead == HOST_ADDRESS_FREE_END) {
		SDL_UnlockMutex(mutex);
		fprintf(
		    stderr,
		    "libPorpoise: host-address token table exhausted; "
		    "no live token was overwritten.\n");
		return 0;
	}

	slot = HostAddressFreeHead;
	entry = &HostAddressSlots[slot];
	HostAddressFreeHead = entry->nextFree;
	entry->pointer = pointer;
	entry->nextFree = HOST_ADDRESS_FREE_END;
	entry->inUse = TRUE;
	token = MakeToken(slot, entry->generation);
	SDL_UnlockMutex(mutex);
	return token;
}

void* __OSHostDecodeAddress(u32 address)
{
	const OSHostMemoryLayout* layout;
	SDL_mutex* mutex;
	OSHostAddressSlot* entry;
	void* pointer;
	u32 slot;
	u32 generation;

	if (address == 0) {
		return NULL;
	}

	if (__OSHostIsAddressToken(address)) {
		slot = address & HOST_ADDRESS_SLOT_MASK;
		generation =
		    (address >> OS_HOST_ADDRESS_TOKEN_SLOT_BITS) &
		    HOST_ADDRESS_GENERATION_MAX;
		if (generation == 0) {
			return NULL;
		}

		mutex = GetHostAddressMutex();
		SDL_LockMutex(mutex);
		entry = &HostAddressSlots[slot];
		pointer =
		    entry->inUse && entry->generation == generation
		        ? (void*)entry->pointer
		        : NULL;
		SDL_UnlockMutex(mutex);
		return pointer;
	}

	layout = __OSHostMemoryGetLayout();
	if (layout == NULL) {
		return NULL;
	}
	if (AddressIsInRange(
	        address,
	        (uintptr_t)layout->cachedBase,
	        layout->size) ||
	    AddressIsInRange(
	        address,
	        (uintptr_t)layout->uncachedBase,
	        layout->size)) {
		return (void*)(uintptr_t)address;
	}
	if (address < layout->size) {
		return (void*)((u8*)layout->cachedBase + address);
	}
	return NULL;
}

void __OSHostReleaseAddress(u32 token)
{
	SDL_mutex* mutex;
	OSHostAddressSlot* entry;
	u32 slot;
	u32 generation;

	if (!__OSHostIsAddressToken(token)) {
		return;
	}

	slot = token & HOST_ADDRESS_SLOT_MASK;
	generation =
	    (token >> OS_HOST_ADDRESS_TOKEN_SLOT_BITS) &
	    HOST_ADDRESS_GENERATION_MAX;
	if (generation == 0) {
		return;
	}

	mutex = GetHostAddressMutex();
	SDL_LockMutex(mutex);
	entry = &HostAddressSlots[slot];
	if (!entry->inUse || entry->generation != generation) {
		SDL_UnlockMutex(mutex);
		return;
	}

	entry->pointer = NULL;
	entry->inUse = FALSE;
	if (entry->generation != HOST_ADDRESS_GENERATION_MAX) {
		++entry->generation;
		entry->nextFree = HostAddressFreeHead;
		HostAddressFreeHead = (u16)slot;
	}
	SDL_UnlockMutex(mutex);
}

#endif
