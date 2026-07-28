#ifndef _DOLPHIN_OS_OSHOSTMEMORY_H
#define _DOLPHIN_OS_OSHOSTMEMORY_H

#include <dolphin/os/OSUtil.h>
#include <dolphin/types.h>

BEGIN_SCOPE_EXTERN_C

#ifdef LIBPORPOISE_PORT

/*
 * Host console-memory profiles describe memory that must retain authentic
 * 32-bit console addresses. Host-only allocations remain unrestricted.
 *
 * Wii MEM1 and MEM2 profiles can be added as additional region layouts
 * without changing callers of this interface.
 */
typedef enum OSHostMemoryProfile {
	OS_HOST_MEMORY_PROFILE_GAMECUBE = 0
} OSHostMemoryProfile;

typedef struct OSHostMemoryLayout {
	OSHostMemoryProfile profile;
	void* cachedBase;
	void* uncachedBase;
	u32 size;
	void* arenaLo;
	void* arenaHi;
} OSHostMemoryLayout;

const OSHostMemoryLayout* __OSHostMemoryInit(
	OSHostMemoryProfile profile);
const OSHostMemoryLayout* __OSHostMemoryGetLayout(void);

#endif

END_SCOPE_EXTERN_C

#endif
