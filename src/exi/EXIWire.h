#ifndef LIBPORPOISE_EXI_WIRE_H
#define LIBPORPOISE_EXI_WIRE_H

#include <dolphin/types.h>

/* EXIImm transfers bytes from most-significant to least-significant order. */
static inline u16 EXIWireRead16(const void* address)
{
	const u8* bytes = (const u8*)address;
	return (u16)(((u16)bytes[0] << 8) | bytes[1]);
}

static inline u32 EXIWireRead32(const void* address)
{
	const u8* bytes = (const u8*)address;
	return ((u32)bytes[0] << 24) | ((u32)bytes[1] << 16) | ((u32)bytes[2] << 8) | bytes[3];
}

static inline void EXIWireWrite16(void* address, u16 value)
{
	u8* bytes = (u8*)address;
	bytes[0]   = (u8)(value >> 8);
	bytes[1]   = (u8)value;
}

static inline void EXIWireWrite32(void* address, u32 value)
{
	u8* bytes = (u8*)address;
	bytes[0]   = (u8)(value >> 24);
	bytes[1]   = (u8)(value >> 16);
	bytes[2]   = (u8)(value >> 8);
	bytes[3]   = (u8)value;
}

static inline void EXIWireMakeGetIDCommand(u8 command[2])
{
	command[0] = 0;
	command[1] = 0;
}

#endif
