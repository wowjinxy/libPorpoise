#include <dolphin/dvd.h>
#include <dolphin/os.h>
#include <string.h>

#include "DVDWire.h"

static u32 status;
static DVDDecodedBB2 bb2;
static u8* bb2Wire;
static DVDDiskID* idTmp;      // also pointer
static u8 bb2Buf[0x3F];

static void* ResolveBB2Address(u32 address)
{
#ifdef LIBPORPOISE_PORT
	return __OSHostDecodeAddress(address);
#else
	return (void*)address;
#endif
}

/**
 * @TODO: Documentation
 */
static void cb(s32 type, DVDCommandBlock* cmdBlock)
{
	if (type > 0) {
		switch (status) {
		case 0:
		{
			status = 1;
			DVDReadAbsAsyncForBS(cmdBlock, bb2Wire, DVD_BB2_WIRE_SIZE, 0x420, cb);
			break;
		}
		case 1:
		{
			void* fstAddress;
			if (!__DVDDecodeBB2(bb2Wire, DVD_BB2_WIRE_SIZE, &bb2)) {
				break;
			}
			fstAddress = ResolveBB2Address(bb2.FSTAddress);
			status = 2;
			DVDReadAbsAsyncForBS(
				cmdBlock,
				fstAddress,
				OSRoundUp32B(bb2.FSTLength),
				bb2.FSTPosition,
				cb);
			break;
		}
		}
	} else if (type == -1) {
	} else if (type == -4) {
		status = 0;
		DVDReset();
		DVDReadDiskID(cmdBlock, idTmp, cb);
	}
}

/**
 * @TODO: Documentation
 */
void __fstLoad(void)
{
	static DVDCommandBlock block;

	int status;
	char* onStr;
	u8 idBuffer[64];
	void* arenaHi;
	void* fstAddress;
	OSBootInfo* bootInfo;
	DVDDiskID* diskID;

	arenaHi = OSGetArenaHi();
	idTmp   = (void*)OSRoundUp32B(idBuffer);
	bb2Wire = (void*)OSRoundUp32B(bb2Buf);
	DVDReset();
	DVDReadDiskID(&block, idTmp, cb);
	do {
		status = DVDGetDriveStatus();
	} while (status != DVD_STATE_END);
	bootInfo = (OSBootInfo*)OS_BASE_CACHED;
	fstAddress = ResolveBB2Address(bb2.FSTAddress);
	bootInfo->FSTLocation = fstAddress;
	bootInfo->FSTMaxLength = bb2.FSTMaxLength;
	diskID = &bootInfo->DVDDiskID;
	memcpy(diskID, idTmp, sizeof(DVDDiskID));
	OSReport("\n");
	OSReport("  Game Name ... %c%c%c%c\n", diskID->gameName[0], diskID->gameName[1], diskID->gameName[2], diskID->gameName[3]);
	OSReport("  Company ..... %c%c\n", diskID->company[0], diskID->company[1]);
	OSReport("  Disk # ...... %d\n", diskID->diskNumber);
	OSReport("  Game ver .... %d\n", diskID->gameVersion);
	if (diskID->streaming == 0) {
		onStr = "OFF";
	} else {
		onStr = "ON";
	}
	OSReport("  Streaming ... %s\n", onStr);
	OSReport("\n");
	OSSetArenaHi(fstAddress);
	return;
}
