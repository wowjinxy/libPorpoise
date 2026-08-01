#include <dolphin/card.h>

#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../src/card/CARDWire.h"

static int Fail(const char* message)
{
	std::fprintf(stderr, "%s\n", message);
	return 1;
}

static bool BytesEqual(const void* actual, const u8* expected, std::size_t length)
{
	return std::memcmp(actual, expected, length) == 0;
}

int main()
{
	static const u8 ChecksumVector[] = { 0x12, 0x34, 0xab, 0xcd, 0x00, 0x01 };
	static const u8 TimeBytes[]      = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
	static const u8 CounterBytes[]   = { 0xa1, 0xb2, 0xc3, 0xd4 };
	static const u8 LanguageBytes[]  = { 0x11, 0x22, 0x33, 0x44 };
	static const u8 DtvBytes[]       = { 0x00, 0x00, 0x55, 0xaa };
	static const u8 IdFields[]       = { 0x00, 0x00, 0x00, 0x3b, 0x00, 0x01 };
	static const u8 ChecksumBytes[]  = { 0x0f, 0xe6, 0xef, 0x1c };
	u16 checksum;
	u16 checksumInv;
	u8 command[2];
	u8 unaligned[10] = {};
	CARDID id;

	CARDWireCalculateChecksum(ChecksumVector, sizeof(ChecksumVector), &checksum, &checksumInv);
	if (checksum != 0xbe02 || checksumInv != 0x41fb) {
		return Fail("CARD checksum did not consume canonical words as big-endian values");
	}

	CARDWireWrite16(unaligned + 1, 0x1234);
	CARDWireWrite32(unaligned + 3, 0x89abcdef);
	if (CARDWireRead16(unaligned + 1) != 0x1234 || CARDWireRead32(unaligned + 3) != 0x89abcdef
	    || unaligned[1] != 0x12 || unaligned[2] != 0x34 || unaligned[3] != 0x89 || unaligned[6] != 0xef) {
		return Fail("CARD scalar wire helpers did not preserve big-endian unaligned bytes");
	}

	if (offsetof(CARDID, deviceID) != 0x20 || offsetof(CARDID, checkSum) != 0x1fc || sizeof(CARDID) != 0x200) {
		return Fail("CARDID layout no longer matches the on-card identity block");
	}
	std::memset(&id, 0, sizeof(id));
	CARDWireWrite64(&id.serial[12], 0x0102030405060708ULL);
	CARDWireWrite32(&id.serial[20], 0xa1b2c3d4);
	CARDWireWrite32(&id.serial[24], 0x11223344);
	CARDWireWrite32(&id.serial[28], 0x000055aa);
	CARDWireWrite16(&id.deviceID, 0);
	CARDWireWrite16(&id.size, 0x003b);
	CARDWireWrite16(&id.encode, CARD_ENCODE_SJIS);

	if (!BytesEqual(&id.serial[12], TimeBytes, sizeof(TimeBytes))
	    || !BytesEqual(&id.serial[20], CounterBytes, sizeof(CounterBytes))
	    || !BytesEqual(&id.serial[24], LanguageBytes, sizeof(LanguageBytes))
	    || !BytesEqual(&id.serial[28], DtvBytes, sizeof(DtvBytes))
	    || !BytesEqual(&id.deviceID, IdFields, sizeof(IdFields))
	    || CARDWireRead64(&id.serial[12]) != 0x0102030405060708ULL
	    || CARDWireRead32(&id.serial[20]) != 0xa1b2c3d4 || CARDWireRead16(&id.size) != 0x003b
	    || CARDWireRead16(&id.encode) != CARD_ENCODE_SJIS) {
		return Fail("CARDID fields did not encode/decode in canonical card byte order");
	}

	CARDWireCalculateChecksum(&id, sizeof(CARDID) - sizeof(u32), &checksum, &checksumInv);
	CARDWireWrite16(&id.checkSum, checksum);
	CARDWireWrite16(&id.checkSumInv, checksumInv);
	if (checksum != 0x0fe6 || checksumInv != 0xef1c
	    || !BytesEqual(&id.checkSum, ChecksumBytes, sizeof(ChecksumBytes))) {
		return Fail("CARDID checksum bytes do not match the canonical identity vector");
	}

	CARDWireMakeReadNintendoIDCommand(command);
	if (command[0] != 0x00 || command[1] != 0x00) {
		return Fail("CARD Nintendo-ID command packet is not 00 00");
	}
	CARDWireMakeEnableInterruptCommand(command, TRUE);
	if (command[0] != 0x81 || command[1] != 0x01) {
		return Fail("CARD interrupt-enable command packet is not 81 01");
	}
	CARDWireMakeEnableInterruptCommand(command, FALSE);
	if (command[0] != 0x81 || command[1] != 0x00) {
		return Fail("CARD interrupt-disable command packet is not 81 00");
	}
	CARDWireMakeReadStatusCommand(command);
	if (command[0] != 0x83 || command[1] != 0x00 || CARDWireMakeClearStatusCommand() != 0x89) {
		return Fail("CARD status command packets do not match the EXI wire protocol");
	}

	return 0;
}
