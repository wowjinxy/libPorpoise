#ifndef _DOLPHIN_OS_OSHOSTADDRESS_H
#define _DOLPHIN_OS_OSHOSTADDRESS_H

#include <dolphin/types.h>

BEGIN_SCOPE_EXTERN_C

#ifdef LIBPORPOISE_PORT

/*
 * Host address policy
 * -------------------
 *
 * 0x00000000-0x017FFFFF  GameCube physical main RAM
 * 0x80000000-0x817FFFFF  GameCube cached main RAM
 * 0xC0000000-0xC17FFFFF  GameCube uncached main RAM
 * 0xB0000000-0xBFFFFFFF  libPorpoise host-address tokens
 *
 * MMIO and other console address spaces retain their hardware-defined
 * addresses. The token range is deliberately separate from physical RAM,
 * mapped RAM, and GameCube/Wii hardware registers.
 */
#define OS_HOST_ADDRESS_TOKEN_TAG        0xB0000000U
#define OS_HOST_ADDRESS_TOKEN_MASK       0xF0000000U
#define OS_HOST_ADDRESS_TOKEN_SLOT_BITS  14U
#define OS_HOST_ADDRESS_TOKEN_SLOT_COUNT (1U << OS_HOST_ADDRESS_TOKEN_SLOT_BITS)

BOOL __OSHostIsAddressToken(u32 address);
/*
 * Return whether the addressed byte is backed by initialized contents from a
 * loaded executable or shared-library file.  Zero-fill image storage (.bss),
 * stack, heap, and emulated console-memory allocations return false.  This
 * lets callers recognize persistent program resources without relying on a
 * particular executable's linker symbols.
 */
BOOL __OSHostIsFileBackedImageAddress(const void* pointer);
u32 __OSHostEncodeAddress(const void* pointer);
/*
 * Encode a native pointer for a 32-bit command or ABI word.  Mapped console
 * pointers and unambiguous low-image pointers remain direct.  Native pointers
 * whose bit pattern overlaps the physical/segmented or token namespaces use
 * the host-address table instead.  Canonical numeric wire addresses should be
 * carried as u32 values rather than cast to pointers.
 */
u32 __OSHostEncodePointerWord(const void* pointer);
void* __OSHostDecodeAddress(u32 address);
void __OSHostReleaseAddress(u32 token);

#endif

END_SCOPE_EXTERN_C

#endif
