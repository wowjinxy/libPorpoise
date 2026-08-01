#ifndef LIBPORPOISE_DVD_WIRE_H
#define LIBPORPOISE_DVD_WIRE_H

#include <dolphin/types.h>

#include <stddef.h>

enum { DVD_BB2_WIRE_SIZE = 0x20 };

/* Host-native values decoded from the fixed-width big-endian BB2 block. */
typedef struct DVDDecodedBB2 {
	u32 bootFilePosition;
	u32 FSTPosition;
	u32 FSTLength;
	u32 FSTMaxLength;
	u32 FSTAddress;
	u32 userPosition;
	u32 userLength;
	u32 reserved;
} DVDDecodedBB2;

static inline u32 __DVDWireReadBigEndian32(const u8* bytes)
{
	return ((u32)bytes[0] << 24) |
	       ((u32)bytes[1] << 16) |
	       ((u32)bytes[2] << 8) |
	       (u32)bytes[3];
}

static inline BOOL __DVDDecodeBB2(
	const void* source,
	size_t sourceSize,
	DVDDecodedBB2* decoded)
{
	const u8* bytes = (const u8*)source;

	if (bytes == NULL || decoded == NULL || sourceSize < DVD_BB2_WIRE_SIZE) {
		return FALSE;
	}

	decoded->bootFilePosition = __DVDWireReadBigEndian32(bytes + 0x00);
	decoded->FSTPosition = __DVDWireReadBigEndian32(bytes + 0x04);
	decoded->FSTLength = __DVDWireReadBigEndian32(bytes + 0x08);
	decoded->FSTMaxLength = __DVDWireReadBigEndian32(bytes + 0x0C);
	decoded->FSTAddress = __DVDWireReadBigEndian32(bytes + 0x10);
	decoded->userPosition = __DVDWireReadBigEndian32(bytes + 0x14);
	decoded->userLength = __DVDWireReadBigEndian32(bytes + 0x18);
	decoded->reserved = __DVDWireReadBigEndian32(bytes + 0x1C);
	return TRUE;
}

#endif /* LIBPORPOISE_DVD_WIRE_H */
