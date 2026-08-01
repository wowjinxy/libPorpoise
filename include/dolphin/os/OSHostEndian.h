#ifndef DOLPHIN_OS_OSHOSTENDIAN_H
#define DOLPHIN_OS_OSHOSTENDIAN_H

#include <dolphin/types.h>

BEGIN_SCOPE_EXTERN_C

/*
 * GameCube scalar data is normally stored in big-endian byte order. These
 * helpers decode and encode unaligned scalar fields without relying on the
 * host CPU's byte order.
 *
 * They must be used at a format-aware boundary. Raw DVD reads intentionally
 * remain byte-for-byte copies because textures, display lists, compressed
 * streams, text, and formats with their own byte order cannot be swapped as
 * an undifferentiated buffer.
 */
u16 OSReadBigEndian16(const void* address);
u32 OSReadBigEndian32(const void* address);
u64 OSReadBigEndian64(const void* address);
s16 OSReadBigEndianS16(const void* address);
s32 OSReadBigEndianS32(const void* address);
s64 OSReadBigEndianS64(const void* address);
f32 OSReadBigEndianF32(const void* address);
f64 OSReadBigEndianF64(const void* address);

/* Decode serialized big-endian elements into native scalar storage. Source
 * and destination may refer to the same buffer. This is intentionally typed:
 * raw file and DVD reads must remain byte-for-byte copies until a format-aware
 * consumer identifies the scalar fields. */
void OSReadBigEndian16Array(
    u16* destination,
    const void* source,
    u32 elementCount);

void OSWriteBigEndian16(void* address, u16 value);
void OSWriteBigEndian32(void* address, u32 value);
void OSWriteBigEndian64(void* address, u64 value);
void OSWriteBigEndianF32(void* address, f32 value);
void OSWriteBigEndianF64(void* address, f64 value);

END_SCOPE_EXTERN_C

#endif
