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

#define GAMECUBE_MEMORY_SIZE 0x01800000U
#define GAMECUBE_ARENA_LO    0x00004000U
#define GAMECUBE_ARENA_HI    0x017FFA80U

static OSHostMemoryLayout HostMemoryLayout;
static BOOL HostMemoryInitialized;

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

static void InitializeGameCubeMemory(void)
{
	void* cached;
	void* uncached;

#if defined(LIBPORPOISE_BUILD_WIN64)
	HostMemoryBacking = CreateFileMappingW(
	    INVALID_HANDLE_VALUE,
	    NULL,
	    PAGE_READWRITE,
	    0,
	    GAMECUBE_MEMORY_SIZE,
	    NULL);
	if (HostMemoryBacking == NULL) {
		FailWindowsMapping(
		    "GameCube backing",
		    OS_BASE_CACHED,
		    GAMECUBE_MEMORY_SIZE,
		    GetLastError());
	}
	cached = MapWindowsView(
	    "GameCube cached",
	    OS_BASE_CACHED,
	    GAMECUBE_MEMORY_SIZE);
	uncached = MapWindowsView(
	    "GameCube uncached",
	    OS_BASE_UNCACHED,
	    GAMECUBE_MEMORY_SIZE);
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
			    GAMECUBE_MEMORY_SIZE,
			    errno);
		}
		shm_unlink(backingName);
		if (ftruncate(HostMemoryBacking, GAMECUBE_MEMORY_SIZE) != 0) {
			FailLinuxMapping(
			    "GameCube backing",
			    OS_BASE_CACHED,
			    GAMECUBE_MEMORY_SIZE,
			    errno);
		}
	}
	cached = MapLinuxView(
	    "GameCube cached",
	    OS_BASE_CACHED,
	    GAMECUBE_MEMORY_SIZE);
	uncached = MapLinuxView(
	    "GameCube uncached",
	    OS_BASE_UNCACHED,
	    GAMECUBE_MEMORY_SIZE);
#else
#error Unsupported libPorpoise host platform
#endif

	memset(cached, 0, GAMECUBE_MEMORY_SIZE);
	HostMemoryLayout.profile = OS_HOST_MEMORY_PROFILE_GAMECUBE;
	HostMemoryLayout.cachedBase = cached;
	HostMemoryLayout.uncachedBase = uncached;
	HostMemoryLayout.size = GAMECUBE_MEMORY_SIZE;
	HostMemoryLayout.arenaLo =
	    (void*)((u8*)cached + GAMECUBE_ARENA_LO);
	HostMemoryLayout.arenaHi =
	    (void*)((u8*)cached + GAMECUBE_ARENA_HI);
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
		InitializeGameCubeMemory();
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

#endif
