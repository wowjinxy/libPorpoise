#if defined(LIBPORPOISE_BUILD_LINUX) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <dolphin/os/OSHostMemory.h>

#ifdef LIBPORPOISE_PORT

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(LIBPORPOISE_BUILD_WIN64)
#include <windows.h>
#elif defined(LIBPORPOISE_BUILD_LINUX)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define GAMECUBE_MEMORY_SIZE          0x01800000U
#define GAMECUBE_EXTENDED_MEMORY_SIZE 0x02000000U
#define GAMECUBE_ARENA_LO             0x00004000U
#define GAMECUBE_ARENA_TOP_RESERVED   0x00000580U

static OSHostMemoryLayout HostMemoryLayout;
static BOOL HostMemoryInitialized;

typedef enum HostArenaHiAdjustmentState {
	HOST_ARENA_HI_WAITING_FOR_INITIAL_VALUE = 0,
	HOST_ARENA_HI_WAITING_FOR_NORMALIZATION,
	HOST_ARENA_HI_ADJUSTMENT_COMPLETE
} HostArenaHiAdjustmentState;

static HostArenaHiAdjustmentState HostArenaHiAdjustment;

#if defined(LIBPORPOISE_BUILD_WIN64)
static HANDLE HostMemoryBacking;

static void FailWindowsMapping(
	const char* viewName,
	uintptr_t address,
	u32 size,
	DWORD error)
{
	fprintf(
	    stderr,
	    "libPorpoise: could not reserve the %s console-memory view "
	    "at 0x%08llX-0x%08llX (%lu bytes). "
	    "Win32 error %lu; another allocation may occupy this range.\n",
	    viewName,
	    (unsigned long long)address,
	    (unsigned long long)(address + size - 1),
	    (unsigned long)size,
	    (unsigned long)error);
	abort();
}

static void* MapWindowsView(
	const char* viewName,
	uintptr_t address,
	u32 size)
{
	void* requested = (void*)address;
	void* mapped = MapViewOfFileEx(
	    HostMemoryBacking,
	    FILE_MAP_ALL_ACCESS,
	    0,
	    0,
	    size,
	    requested);

	if (mapped != requested) {
		DWORD error = GetLastError();
		if (mapped != NULL) {
			UnmapViewOfFile(mapped);
		}
		FailWindowsMapping(viewName, address, size, error);
	}
	return mapped;
}
#elif defined(LIBPORPOISE_BUILD_LINUX)
static int HostMemoryBacking = -1;

static void FailLinuxMapping(
	const char* viewName,
	uintptr_t address,
	u32 size,
	int error)
{
	fprintf(
	    stderr,
	    "libPorpoise: could not reserve the %s console-memory view "
	    "at 0x%08llX-0x%08llX (%u bytes): %s. "
	    "Another allocation may occupy this range.\n",
	    viewName,
	    (unsigned long long)address,
	    (unsigned long long)(address + size - 1),
	    (unsigned int)size,
	    strerror(error));
	abort();
}

static void* MapLinuxView(
	const char* viewName,
	uintptr_t address,
	u32 size)
{
	void* requested = (void*)address;
	void* mapped;
	int error;
	int flags = MAP_SHARED;

#ifdef MAP_FIXED_NOREPLACE
	flags |= MAP_FIXED_NOREPLACE;
#endif
	errno = 0;
	mapped = mmap(
	    requested,
	    size,
	    PROT_READ | PROT_WRITE,
	    flags,
	    HostMemoryBacking,
	    0);
#ifdef MAP_FIXED_NOREPLACE
	if (mapped == MAP_FAILED && errno == EINVAL) {
		/*
		 * Older kernels may reject MAP_FIXED_NOREPLACE even when the build
		 * headers define it. A plain address hint is still non-destructive;
		 * accepting only the exact returned address preserves strictness.
		 */
		mapped = mmap(
		    requested,
		    size,
		    PROT_READ | PROT_WRITE,
		    MAP_SHARED,
		    HostMemoryBacking,
		    0);
	}
#endif
	if (mapped == MAP_FAILED) {
		FailLinuxMapping(viewName, address, size, errno);
	}
	if (mapped != requested) {
		munmap(mapped, size);
		error = EEXIST;
		FailLinuxMapping(viewName, address, size, error);
	}
	return mapped;
}
#endif

static void InitializeGameCubeMemory(
	OSHostMemoryProfile profile,
	u32 memorySize,
	u32 consoleSize)
{
	void* cached;
	void* uncached;

#if defined(LIBPORPOISE_BUILD_WIN64)
	HostMemoryBacking = CreateFileMappingW(
	    INVALID_HANDLE_VALUE,
	    NULL,
	    PAGE_READWRITE,
	    0,
	    memorySize,
	    NULL);
	if (HostMemoryBacking == NULL) {
		FailWindowsMapping(
		    "GameCube backing",
		    OS_BASE_CACHED,
		    memorySize,
		    GetLastError());
	}
	cached = MapWindowsView(
	    "GameCube cached",
	    OS_BASE_CACHED,
	    memorySize);
	uncached = MapWindowsView(
	    "GameCube uncached",
	    OS_BASE_UNCACHED,
	    memorySize);
#elif defined(LIBPORPOISE_BUILD_LINUX)
	{
		char backingName[96];

		snprintf(
		    backingName,
		    sizeof(backingName),
		    "/libporpoise-%ld-%p",
		    (long)getpid(),
		    (void*)&HostMemoryLayout);
		HostMemoryBacking = shm_open(
		    backingName,
		    O_CREAT | O_EXCL | O_RDWR,
		    S_IRUSR | S_IWUSR);
		if (HostMemoryBacking < 0) {
			FailLinuxMapping(
			    "GameCube backing",
			    OS_BASE_CACHED,
			    memorySize,
			    errno);
		}
		shm_unlink(backingName);
		if (ftruncate(HostMemoryBacking, memorySize) != 0) {
			FailLinuxMapping(
			    "GameCube backing",
			    OS_BASE_CACHED,
			    memorySize,
			    errno);
		}
	}
	cached = MapLinuxView(
	    "GameCube cached",
	    OS_BASE_CACHED,
	    memorySize);
	uncached = MapLinuxView(
	    "GameCube uncached",
	    OS_BASE_UNCACHED,
	    memorySize);
#else
#error Unsupported libPorpoise host platform
#endif

	memset(cached, 0, memorySize);
	HostMemoryLayout.profile = profile;
	HostMemoryLayout.cachedBase = cached;
	HostMemoryLayout.uncachedBase = uncached;
	HostMemoryLayout.size = memorySize;
	HostMemoryLayout.consoleSize = consoleSize;
	HostMemoryLayout.arenaLo =
	    (void*)((u8*)cached + GAMECUBE_ARENA_LO);
	HostMemoryLayout.arenaHi =
	    (void*)((u8*)cached + memorySize - GAMECUBE_ARENA_TOP_RESERVED);
	HostMemoryLayout.consoleArenaHi =
	    (void*)((u8*)cached + consoleSize - GAMECUBE_ARENA_TOP_RESERVED);
}

const OSHostMemoryLayout* __OSHostMemoryInit(
	OSHostMemoryProfile profile)
{
	if (HostMemoryInitialized) {
		if (HostMemoryLayout.profile != profile) {
			fprintf(
			    stderr,
			    "libPorpoise: console memory is already initialized "
			    "with profile %d; profile %d cannot be selected.\n",
			    (int)HostMemoryLayout.profile,
			    (int)profile);
			abort();
		}
		return &HostMemoryLayout;
	}

	switch (profile) {
	case OS_HOST_MEMORY_PROFILE_GAMECUBE:
		InitializeGameCubeMemory(
		    profile,
		    GAMECUBE_MEMORY_SIZE,
		    GAMECUBE_MEMORY_SIZE);
		break;
	case OS_HOST_MEMORY_PROFILE_GAMECUBE_EXTENDED:
		InitializeGameCubeMemory(
		    profile,
		    GAMECUBE_EXTENDED_MEMORY_SIZE,
		    GAMECUBE_MEMORY_SIZE);
		break;
	default:
		fprintf(
		    stderr,
		    "libPorpoise: unsupported host console-memory profile %d.\n",
		    (int)profile);
		abort();
	}

	HostMemoryInitialized = TRUE;
	return &HostMemoryLayout;
}

const OSHostMemoryLayout* __OSHostMemoryGetLayout(void)
{
	return HostMemoryInitialized ? &HostMemoryLayout : NULL;
}

BOOL __OSHostMemoryContainsAddress(const void* address)
{
	uintptr_t value;
	uintptr_t cachedBase;
	uintptr_t uncachedBase;

	if (!HostMemoryInitialized || address == NULL) {
		return FALSE;
	}

	value = (uintptr_t)address;
	cachedBase = (uintptr_t)HostMemoryLayout.cachedBase;
	uncachedBase = (uintptr_t)HostMemoryLayout.uncachedBase;
	return (value >= cachedBase &&
	        value - cachedBase < HostMemoryLayout.size) ||
	       (value >= uncachedBase &&
	        value - uncachedBase < HostMemoryLayout.size);
}

void* __OSHostMemoryResolveArenaHi(void* previous, void* requested)
{
	if (!HostMemoryInitialized ||
	    HostMemoryLayout.profile !=
	        OS_HOST_MEMORY_PROFILE_GAMECUBE_EXTENDED) {
		return requested;
	}

	switch (HostArenaHiAdjustment) {
	case HOST_ARENA_HI_WAITING_FOR_INITIAL_VALUE:
		/*
		 * OSInit publishes the profile's initial extended ceiling from a
		 * zero-initialized arena. Only that exact transition arms the
		 * compatibility adjustment.
		 */
		if (previous == NULL && requested == HostMemoryLayout.arenaHi) {
			HostArenaHiAdjustment =
			    HOST_ARENA_HI_WAITING_FOR_NORMALIZATION;
		} else {
			HostArenaHiAdjustment =
			    HOST_ARENA_HI_ADJUSTMENT_COMPLETE;
		}
		return requested;

	case HOST_ARENA_HI_WAITING_FOR_NORMALIZATION:
		/* Redundant publication of the initial ceiling is harmless. */
		if (previous == HostMemoryLayout.arenaHi &&
		    requested == HostMemoryLayout.arenaHi) {
			return requested;
		}

		HostArenaHiAdjustment = HOST_ARENA_HI_ADJUSTMENT_COMPLETE;
		if (previous == HostMemoryLayout.arenaHi &&
		    requested == HostMemoryLayout.consoleArenaHi) {
			/*
			 * Console startup code may normalize ArenaHi from the host
			 * ceiling to the authentic 24 MiB ceiling. Preserve the host
			 * overhead once; all later heap operations pass through exactly.
			 */
			return HostMemoryLayout.arenaHi;
		}
		return requested;

	case HOST_ARENA_HI_ADJUSTMENT_COMPLETE:
	default:
		return requested;
	}
}

#endif
