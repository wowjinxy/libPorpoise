#ifndef LIBPORPOISE_SI_WIRE_H
#define LIBPORPOISE_SI_WIRE_H

#include <dolphin/types.h>

/* SI buffers are byte streams consumed by big-endian hardware registers.
 * These helpers deliberately avoid scalar aliases so unaligned and partial
 * transfers have the same representation on PowerPC and little-endian hosts. */
static inline u32 __SIPackWireWord(const void* source, u32 byteCount)
{
	const u8* bytes = (const u8*)source;
	u32 word = 0;
	u32 index;

	if (byteCount > 4) {
		byteCount = 4;
	}
	for (index = 0; index < byteCount; ++index) {
		word |= (u32)bytes[index] << (24 - index * 8);
	}
	return word;
}

static inline void __SIUnpackWireWord(void* destination, u32 byteCount,
	                                  u32 word)
{
	u8* bytes = (u8*)destination;
	u32 index;

	if (byteCount > 4) {
		byteCount = 4;
	}
	for (index = 0; index < byteCount; ++index) {
		bytes[index] = (u8)(word >> (24 - index * 8));
	}
}

/* SDK code commonly passes the address of a native u32 while transferring
 * only its most-significant one to three bytes.  That works naturally on the
 * big-endian console, but not on a little-endian host.  Keep that adaptation
 * explicit at the call site instead of guessing the pointed-to type in the
 * public byte-buffer API. */
static inline void __SIEncodeHostU32Prefix(void* destination, u32 byteCount,
	                                       u32 value)
{
	__SIUnpackWireWord(destination, byteCount, value);
}

static inline u32 __SIDecodeHostU32Prefix(const void* source, u32 byteCount)
{
	return __SIPackWireWord(source, byteCount);
}

static inline u32 __SIBuildComcsr(u32 current, u32 channel,
	                               u32 outputBytes, u32 inputBytes,
	                               BOOL callbackEnabled)
{
	enum {
		SI_COMCSR_TRANSFER_FIELDS = 0xC07F7F07u,
		SI_COMCSR_TCINT = 0x80000000u,
		SI_COMCSR_TCINTMSK = 0x40000000u,
		SI_COMCSR_TSTART = 0x00000001u
	};

	current &= ~(u32)SI_COMCSR_TRANSFER_FIELDS;
	current |= SI_COMCSR_TCINT | SI_COMCSR_TSTART;
	if (callbackEnabled) {
		current |= SI_COMCSR_TCINTMSK;
	}
	current |= (outputBytes & 0x7fu) << 16;
	current |= (inputBytes & 0x7fu) << 8;
	current |= (channel & 3u) << 1;
	return current;
}

#endif
