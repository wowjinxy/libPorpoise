#ifndef LIBPORPOISE_PAD_WIRE_H
#define LIBPORPOISE_PAD_WIRE_H

#include <dolphin/pad.h>

#include "../si/SIWire.h"

typedef struct PADHostTransferStaging {
	u8 command[3];
	u8 response[10];
} PADHostTransferStaging;

static inline void* __PADStageScalarCommand(
	PADHostTransferStaging* staging,
	u32 command,
	u32 byteCount)
{
	__SIEncodeHostU32Prefix(staging->command, byteCount, command);
	return staging->command;
}

static inline void* __PADStageResponse(PADHostTransferStaging* staging)
{
	return staging->response;
}

static inline u32 __PADCommitScalarResponse(
	const PADHostTransferStaging* staging,
	u32 byteCount)
{
	return __SIDecodeHostU32Prefix(staging->response, byteCount);
}

static inline void __PADCommitOriginResponse(
	PADStatus* origin,
	const PADHostTransferStaging* staging,
	u32 byteCount)
{
	const u8* response = staging->response;

	if (byteCount >= 2) {
		origin->button = (u16)(((u16)response[0] << 8) | response[1]);
	}
	if (byteCount >= 3) {
		origin->stickX = (s8)response[2];
	}
	if (byteCount >= 4) {
		origin->stickY = (s8)response[3];
	}
	if (byteCount >= 5) {
		origin->substickX = (s8)response[4];
	}
	if (byteCount >= 6) {
		origin->substickY = (s8)response[5];
	}
	if (byteCount >= 7) {
		origin->triggerLeft = response[6];
	}
	if (byteCount >= 8) {
		origin->triggerRight = response[7];
	}
	if (byteCount >= 9) {
		origin->analogA = response[8];
	}
	if (byteCount >= 10) {
		origin->analogB = response[9];
	}
}

#endif
