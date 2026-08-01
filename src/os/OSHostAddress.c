#if defined(LIBPORPOISE_BUILD_LINUX) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <dolphin/os/OSHostAddress.h>

#ifdef LIBPORPOISE_PORT

#include <dolphin/os/OSHostMemory.h>
#include <SDL2/SDL_atomic.h>
#include <SDL2/SDL_mutex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(LIBPORPOISE_BUILD_WIN64)
#include <windows.h>
#elif defined(LIBPORPOISE_BUILD_LINUX)
#include <link.h>
#endif

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

#if defined(LIBPORPOISE_BUILD_LINUX)
typedef struct OSHostImageAddressQuery {
	uintptr_t address;
	BOOL isFileBacked;
} OSHostImageAddressQuery;

static int ClassifyImageProgramHeader(
	struct dl_phdr_info* image,
	size_t size,
	void* argument)
{
	OSHostImageAddressQuery* query = argument;
	ElfW(Half) index;

	(void)size;
	for (index = 0; index < image->dlpi_phnum; ++index) {
		const ElfW(Phdr)* header = &image->dlpi_phdr[index];
		uintptr_t start;
		uintptr_t offset;

		if (header->p_type != PT_LOAD) {
			continue;
		}
		start = (uintptr_t)image->dlpi_addr +
		        (uintptr_t)header->p_vaddr;
		if (query->address < start) {
			continue;
		}
		offset = query->address - start;
		if (offset >= (uintptr_t)header->p_memsz) {
			continue;
		}

		/* p_memsz extends p_filesz with the segment's zero-fill tail. */
		query->isFileBacked = offset < (uintptr_t)header->p_filesz;
		return 1;
	}
	return 0;
}
#endif

BOOL __OSHostIsFileBackedImageAddress(const void* pointer)
{
	if (pointer == NULL) {
		return FALSE;
	}

#if defined(LIBPORPOISE_BUILD_WIN64)
	{
		MEMORY_BASIC_INFORMATION info;
		const u8* imageBase;
		const IMAGE_DOS_HEADER* dosHeader;
		const IMAGE_NT_HEADERS* ntHeaders;
		const IMAGE_SECTION_HEADER* section;
		uintptr_t imageOffset;
		u16 index;

		if (VirtualQuery(pointer, &info, sizeof(info)) != sizeof(info)) {
			return FALSE;
		}
		if (info.State != MEM_COMMIT || info.Type != MEM_IMAGE ||
		    info.AllocationBase == NULL) {
			return FALSE;
		}

		imageBase = (const u8*)info.AllocationBase;
		dosHeader = (const IMAGE_DOS_HEADER*)imageBase;
		if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE ||
		    dosHeader->e_lfanew < 0) {
			return FALSE;
		}
		ntHeaders = (const IMAGE_NT_HEADERS*)(
		    imageBase + (uintptr_t)dosHeader->e_lfanew);
		if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
			return FALSE;
		}

		imageOffset = (uintptr_t)pointer - (uintptr_t)imageBase;
		section = IMAGE_FIRST_SECTION(ntHeaders);
		for (index = 0; index < ntHeaders->FileHeader.NumberOfSections;
		     ++index, ++section) {
			uintptr_t sectionOffset;
			u32 virtualSize = section->Misc.VirtualSize;

			if (virtualSize == 0) {
				virtualSize = section->SizeOfRawData;
			}
			if (imageOffset < section->VirtualAddress) {
				continue;
			}
			sectionOffset = imageOffset - section->VirtualAddress;
			if (sectionOffset >= virtualSize) {
				continue;
			}

			return sectionOffset < section->SizeOfRawData &&
			       (section->Characteristics &
			        IMAGE_SCN_CNT_UNINITIALIZED_DATA) == 0;
		}
		return FALSE;
	}
#elif defined(LIBPORPOISE_BUILD_LINUX)
	{
		OSHostImageAddressQuery query;

		query.address = (uintptr_t)pointer;
		query.isFileBacked = FALSE;
		dl_iterate_phdr(ClassifyImageProgramHeader, &query);
		return query.isFileBacked;
	}
#else
	return FALSE;
#endif
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

u32 __OSHostEncodePointerWord(const void* pointer)
{
	uintptr_t address;
	u32 word;

	if (pointer == NULL) {
		return 0;
	}

	address = (uintptr_t)pointer;
	if (address <= UINT32_MAX) {
		word = (u32)address;

		/*
		 * A nonzero high nibble is an unambiguous direct address for the
		 * host consumers, except for libPorpoise's own token namespace.
		 * This preserves fixed-image pointers and mapped cached/uncached
		 * console pointers without allocating a token.
		 *
		 * High-nibble-zero values overlap physical and segmented command
		 * addresses.  A real native pointer in that range must therefore be
		 * tokenized; numeric wire addresses never call this pointer API.
		 */
		if ((word & OS_HOST_ADDRESS_TOKEN_MASK) != 0 &&
		    !__OSHostIsAddressToken(word)) {
			return word;
		}
	}

	return __OSHostEncodeAddress(pointer);
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
