#ifndef LIBPORPOISE_OS_RTC_WIRE_H
#define LIBPORPOISE_OS_RTC_WIRE_H

#include <dolphin/os/OSRtc.h>

#include "../exi/EXIWire.h"

static inline void OSRtcWireDecodeSramImage(const u8 wire[RTC_SRAM_SIZE], void* nativeImage)
{
	u8* nativeBytes = (u8*)nativeImage;
	OSSram* sram;
	OSSramEx* sramEx;
	int i;

	for (i = 0; i < RTC_SRAM_SIZE; ++i) {
		nativeBytes[i] = wire[i];
	}

	sram           = (OSSram*)nativeImage;
	sram->checkSum    = EXIWireRead16(wire + 0x00);
	sram->checkSumInv = EXIWireRead16(wire + 0x02);
	sram->ead0        = EXIWireRead32(wire + 0x04);
	sram->ead1        = EXIWireRead32(wire + 0x08);
	sram->counterBias = EXIWireRead32(wire + 0x0c);

	sramEx = (OSSramEx*)(nativeBytes + sizeof(OSSram));
	sramEx->wirelessKeyboardID = EXIWireRead32(wire + 0x2c);
	for (i = 0; i < 4; ++i) {
		sramEx->wirelessPadID[i] = EXIWireRead16(wire + 0x30 + i * sizeof(u16));
	}
	sramEx->gbs = EXIWireRead16(wire + 0x3c);
}

static inline void OSRtcWireEncodeSramImage(const void* nativeImage, u8 wire[RTC_SRAM_SIZE])
{
	const u8* nativeBytes = (const u8*)nativeImage;
	const OSSram* sram;
	const OSSramEx* sramEx;
	int i;

	for (i = 0; i < RTC_SRAM_SIZE; ++i) {
		wire[i] = nativeBytes[i];
	}

	sram = (const OSSram*)nativeImage;
	EXIWireWrite16(wire + 0x00, sram->checkSum);
	EXIWireWrite16(wire + 0x02, sram->checkSumInv);
	EXIWireWrite32(wire + 0x04, sram->ead0);
	EXIWireWrite32(wire + 0x08, sram->ead1);
	EXIWireWrite32(wire + 0x0c, sram->counterBias);

	sramEx = (const OSSramEx*)(nativeBytes + sizeof(OSSram));
	EXIWireWrite32(wire + 0x2c, sramEx->wirelessKeyboardID);
	for (i = 0; i < 4; ++i) {
		EXIWireWrite16(wire + 0x30 + i * sizeof(u16), sramEx->wirelessPadID[i]);
	}
	EXIWireWrite16(wire + 0x3c, sramEx->gbs);
}

static inline void OSRtcWireCalculateChecksum(const OSSram* sram, u16* checkSum, u16* checkSumInv)
{
	u16 words[4];
	u16 sum     = 0;
	u16 inverse = 0;
	int i;

	words[0] = (u16)(sram->counterBias >> 16);
	words[1] = (u16)sram->counterBias;
	words[2] = (u16)(((u16)(u8)sram->displayOffsetH << 8) | sram->ntd);
	words[3] = (u16)(((u16)sram->language << 8) | sram->flags);
	for (i = 0; i < 4; ++i) {
		sum     = (u16)(sum + words[i]);
		inverse = (u16)(inverse + (u16)~words[i]);
	}
	*checkSum    = sum;
	*checkSumInv = inverse;
}

static inline void OSRtcWireMakeReadSramCommand(u8 command[4])
{
	EXIWireWrite32(command, RTC_CMD_READ | RTC_SRAM_ADDR);
}

static inline void OSRtcWireMakeWriteSramCommand(u8 command[4], u32 offset)
{
	EXIWireWrite32(command, RTC_CMD_WRITE | (RTC_SRAM_ADDR + (offset << 6)));
}

#endif
