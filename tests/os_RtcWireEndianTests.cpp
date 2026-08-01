#include <dolphin/os/OSRtc.h>

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../src/exi/EXIWire.h"
#include "../src/os/OSRtcWire.h"

namespace {

int Fail(const char* message)
{
	std::fprintf(stderr, "%s\n", message);
	return 1;
}

bool BytesEqual(const void* actual, const void* expected, std::size_t size)
{
	return std::memcmp(actual, expected, size) == 0;
}

} // namespace

int main()
{
	static_assert(sizeof(OSSram) == 0x14, "OSSram must retain its SDK layout");
	static_assert(sizeof(OSSramEx) == 0x2c, "OSSramEx must retain its SDK layout");

	std::array<u8, RTC_SRAM_SIZE> wire{};
	for (std::size_t i = 0; i < wire.size(); ++i) {
		wire[i] = static_cast<u8>(0x40u + i);
	}
	const u8 scalarBytes[] = {
		0xa1, 0xb2, 0xc3, 0xd4, // checksums
		0x11, 0x22, 0x33, 0x44, // ead0
		0x55, 0x66, 0x77, 0x88, // ead1
		0x01, 0x02, 0x03, 0x04, // counter bias
		0xfe, 0x55, 0x04, 0x84, // display offset, ntd, language, flags
	};
	std::memcpy(wire.data(), scalarBytes, sizeof(scalarBytes));
	const u8 exScalarBytes[] = {
		0x89, 0xab, 0xcd, 0xef, // wireless keyboard ID
		0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, // pad IDs
	};
	std::memcpy(wire.data() + 0x2c, exScalarBytes, sizeof(exScalarBytes));
	wire[0x3c] = 0xbe;
	wire[0x3d] = 0xef;

	alignas(OSSramEx) std::array<u8, RTC_SRAM_SIZE> native{};
	OSRtcWireDecodeSramImage(wire.data(), native.data());
	const OSSram* sram = reinterpret_cast<const OSSram*>(native.data());
	const OSSramEx* sramEx = reinterpret_cast<const OSSramEx*>(native.data() + sizeof(OSSram));
	if (sram->checkSum != 0xa1b2 || sram->checkSumInv != 0xc3d4 || sram->ead0 != 0x11223344
	    || sram->ead1 != 0x55667788 || sram->counterBias != 0x01020304 || sram->displayOffsetH != -2
	    || sram->ntd != 0x55 || sram->language != 0x04 || sram->flags != 0x84) {
		return Fail("RTC SRAM base fields were not decoded from canonical big-endian bytes");
	}
	if (sramEx->wirelessKeyboardID != 0x89abcdef || sramEx->wirelessPadID[0] != 0x1020
	    || sramEx->wirelessPadID[1] != 0x3040 || sramEx->wirelessPadID[2] != 0x5060
	    || sramEx->wirelessPadID[3] != 0x7080 || sramEx->gbs != 0xbeef
	    || !BytesEqual(sramEx->flashID, wire.data() + sizeof(OSSram), sizeof(sramEx->flashID))) {
		return Fail("RTC SRAM extension fields were not decoded from canonical big-endian bytes");
	}

	std::array<u8, RTC_SRAM_SIZE> roundTrip{};
	OSRtcWireEncodeSramImage(native.data(), roundTrip.data());
	if (roundTrip != wire) {
		return Fail("RTC SRAM decode/encode did not preserve the exact 64-byte image");
	}

	u16 checkSum;
	u16 checkSumInv;
	OSRtcWireCalculateChecksum(sram, &checkSum, &checkSumInv);
	if (checkSum != 0x06df || checkSumInv != 0xf91d) {
		return Fail("RTC SRAM checksum did not use the canonical big-endian word sequence");
	}

	u8 unaligned[9]{};
	EXIWireWrite16(unaligned + 1, 0x1234);
	EXIWireWrite32(unaligned + 3, 0x89abcdef);
	if (EXIWireRead16(unaligned + 1) != 0x1234 || EXIWireRead32(unaligned + 3) != 0x89abcdef
	    || unaligned[1] != 0x12 || unaligned[2] != 0x34 || unaligned[3] != 0x89 || unaligned[6] != 0xef) {
		return Fail("EXI scalar helpers did not preserve big-endian unaligned bytes");
	}

	u8 command[4]{};
	const u8 getIdCommand[] = { 0x00, 0x00 };
	EXIWireMakeGetIDCommand(command);
	if (!BytesEqual(command, getIdCommand, sizeof(getIdCommand))) {
		return Fail("EXI GetID command is not 00 00");
	}

	const u8 readSramCommand[] = { 0x20, 0x00, 0x01, 0x00 };
	OSRtcWireMakeReadSramCommand(command);
	if (!BytesEqual(command, readSramCommand, sizeof(readSramCommand))) {
		return Fail("RTC SRAM read command is not 20 00 01 00");
	}

	const u8 writeSramCommand[] = { 0xa0, 0x00, 0x06, 0x00 };
	OSRtcWireMakeWriteSramCommand(command, sizeof(OSSram));
	if (!BytesEqual(command, writeSramCommand, sizeof(writeSramCommand))) {
		return Fail("RTC SRAM extension write command is not A0 00 06 00");
	}

	const u8 uartQueueCommand[] = { 0x20, 0x01, 0x00, 0x00 };
	EXIWireWrite32(command, 0x00800400u << 6);
	if (!BytesEqual(command, uartQueueCommand, sizeof(uartQueueCommand))) {
		return Fail("EXI UART queue command is not 20 01 00 00");
	}

	const u8 uartWriteCommand[] = { 0xa0, 0x01, 0x00, 0x00 };
	EXIWireWrite32(command, (0x00800400u | 0x02000000u) << 6);
	if (!BytesEqual(command, uartWriteCommand, sizeof(uartWriteCommand))) {
		return Fail("EXI UART write command is not A0 01 00 00");
	}

	return 0;
}
