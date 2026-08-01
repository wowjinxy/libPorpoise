#ifndef LIBPORPOISE_CARD_WIRE_H
#define LIBPORPOISE_CARD_WIRE_H

#include <dolphin/types.h>

/*
 * Memory-card system blocks and EXI command/response packets use the
 * GameCube's big-endian byte order.  Keep those buffers in wire order and
 * convert only when a scalar crosses into native host code.
 */
static inline u16 CARDWireRead16(const void* address)
{
	const u8* bytes = (const u8*)address;
	return (u16)(((u16)bytes[0] << 8) | bytes[1]);
}

static inline s16 CARDWireReadS16(const void* address)
{
	return (s16)CARDWireRead16(address);
}

static inline u32 CARDWireRead32(const void* address)
{
	const u8* bytes = (const u8*)address;
	return ((u32)bytes[0] << 24) | ((u32)bytes[1] << 16) | ((u32)bytes[2] << 8) | bytes[3];
}

static inline u64 CARDWireRead64(const void* address)
{
	const u8* bytes = (const u8*)address;
	return ((u64)bytes[0] << 56) | ((u64)bytes[1] << 48) | ((u64)bytes[2] << 40) | ((u64)bytes[3] << 32)
	       | ((u64)bytes[4] << 24) | ((u64)bytes[5] << 16) | ((u64)bytes[6] << 8) | bytes[7];
}

static inline void CARDWireWrite16(void* address, u16 value)
{
	u8* bytes = (u8*)address;
	bytes[0]   = (u8)(value >> 8);
	bytes[1]   = (u8)value;
}

static inline void CARDWireWrite32(void* address, u32 value)
{
	u8* bytes = (u8*)address;
	bytes[0]   = (u8)(value >> 24);
	bytes[1]   = (u8)(value >> 16);
	bytes[2]   = (u8)(value >> 8);
	bytes[3]   = (u8)value;
}

static inline void CARDWireWrite64(void* address, u64 value)
{
	u8* bytes = (u8*)address;
	bytes[0]   = (u8)(value >> 56);
	bytes[1]   = (u8)(value >> 48);
	bytes[2]   = (u8)(value >> 40);
	bytes[3]   = (u8)(value >> 32);
	bytes[4]   = (u8)(value >> 24);
	bytes[5]   = (u8)(value >> 16);
	bytes[6]   = (u8)(value >> 8);
	bytes[7]   = (u8)value;
}

static inline void CARDWireCalculateChecksum(const void* data, int length, u16* checksum, u16* checksumInv)
{
	const u8* bytes = (const u8*)data;
	u16 sum         = 0;
	u16 inverse     = 0;
	int offset;

	for (offset = 0; offset + 1 < length; offset += 2) {
		u16 word = CARDWireRead16(bytes + offset);
		sum      = (u16)(sum + word);
		inverse  = (u16)(inverse + (u16)~word);
	}
	if (sum == 0xffff) {
		sum = 0;
	}
	if (inverse == 0xffff) {
		inverse = 0;
	}
	*checksum    = sum;
	*checksumInv = inverse;
}

static inline void CARDWireMakeReadNintendoIDCommand(u8 command[2])
{
	command[0] = 0x00;
	command[1] = 0x00;
}

static inline void CARDWireMakeEnableInterruptCommand(u8 command[2], BOOL enable)
{
	command[0] = 0x81;
	command[1] = enable ? 0x01 : 0x00;
}

static inline void CARDWireMakeReadStatusCommand(u8 command[2])
{
	command[0] = 0x83;
	command[1] = 0x00;
}

static inline u8 CARDWireMakeClearStatusCommand(void)
{
	return 0x89;
}

#endif
