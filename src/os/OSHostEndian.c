#include <dolphin/os/OSHostEndian.h>

#include <string.h>

u16 OSReadBigEndian16(const void* address)
{
	const u8* bytes = (const u8*)address;
	return ((u16)bytes[0] << 8) | (u16)bytes[1];
}

u32 OSReadBigEndian32(const void* address)
{
	const u8* bytes = (const u8*)address;
	return ((u32)bytes[0] << 24) | ((u32)bytes[1] << 16)
	     | ((u32)bytes[2] << 8) | (u32)bytes[3];
}

u64 OSReadBigEndian64(const void* address)
{
	const u8* bytes = (const u8*)address;
	return ((u64)bytes[0] << 56) | ((u64)bytes[1] << 48)
	     | ((u64)bytes[2] << 40) | ((u64)bytes[3] << 32)
	     | ((u64)bytes[4] << 24) | ((u64)bytes[5] << 16)
	     | ((u64)bytes[6] << 8) | (u64)bytes[7];
}

s16 OSReadBigEndianS16(const void* address)
{
	return (s16)OSReadBigEndian16(address);
}

s32 OSReadBigEndianS32(const void* address)
{
	return (s32)OSReadBigEndian32(address);
}

s64 OSReadBigEndianS64(const void* address)
{
	return (s64)OSReadBigEndian64(address);
}

f32 OSReadBigEndianF32(const void* address)
{
	u32 bits = OSReadBigEndian32(address);
	f32 value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

void OSWriteBigEndian16(void* address, u16 value)
{
	u8* bytes = (u8*)address;
	bytes[0] = (u8)(value >> 8);
	bytes[1] = (u8)value;
}

void OSWriteBigEndian32(void* address, u32 value)
{
	u8* bytes = (u8*)address;
	bytes[0] = (u8)(value >> 24);
	bytes[1] = (u8)(value >> 16);
	bytes[2] = (u8)(value >> 8);
	bytes[3] = (u8)value;
}

void OSWriteBigEndian64(void* address, u64 value)
{
	u8* bytes = (u8*)address;
	bytes[0] = (u8)(value >> 56);
	bytes[1] = (u8)(value >> 48);
	bytes[2] = (u8)(value >> 40);
	bytes[3] = (u8)(value >> 32);
	bytes[4] = (u8)(value >> 24);
	bytes[5] = (u8)(value >> 16);
	bytes[6] = (u8)(value >> 8);
	bytes[7] = (u8)value;
}

void OSWriteBigEndianF32(void* address, f32 value)
{
	u32 bits;
	memcpy(&bits, &value, sizeof(bits));
	OSWriteBigEndian32(address, bits);
}

void OSWriteBigEndianF64(void* address, f64 value)
{
	u64 bits;
	memcpy(&bits, &value, sizeof(bits));
	OSWriteBigEndian64(address, bits);
}

f64 OSReadBigEndianF64(const void* address)
{
	u64 bits = OSReadBigEndian64(address);
	f64 value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}
