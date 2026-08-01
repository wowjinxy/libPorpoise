#include <dolphin/os/OSArena.h>
#ifdef LIBPORPOISE_PORT
#include <dolphin/os/OSHostMemory.h>
#endif

static void* __OSArenaHi = (void*)0x00000000;
static void* __OSArenaLo = (void*)0xffffffff;

/**
 * @TODO: Documentation
 */
void* OSGetArenaHi(void)
{
	return __OSArenaHi;
}

/**
 * @TODO: Documentation
 */
void* OSGetArenaLo(void)
{
	return __OSArenaLo;
}

/**
 * @TODO: Documentation
 */
void OSSetArenaHi(void* arena_hi)
{
#ifdef LIBPORPOISE_PORT
	arena_hi = __OSHostMemoryResolveArenaHi(__OSArenaHi, arena_hi);
#endif
	__OSArenaHi = arena_hi;
	return;
}

/**
 * @TODO: Documentation
 */
void OSSetArenaLo(void* arena_lo)
{
	__OSArenaLo = arena_lo;
	return;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00002C
 */
void OSAllocFromArenaLo(void)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000020
 */
void OSAllocFromArenaHi(void)
{
	TRAP_UNIMPLEMENTED;
}
