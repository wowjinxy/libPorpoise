#include <dolphin/card.h>

#include <cstdio>
#include <cstring>

#include "../src/card/CARDWire.h"

static CARDDirectoryBlock Directory;
static s32 ControlResult;
static s32 AccessResult;
static s32 PublicResult;
static s32 FileLookupResult;
static s32 FileLookupNumber;
static s32 UpdateResult;
static s32 FreeResult;
static s32 SyncResult;
static BOOL FileOpened;
static int PutCount;
static int UpdateCount;
static int FreeCount;
static int SyncCount;
static int UserCallbackCount;
static int DefaultCallbackCount;
static int SyncCallbackCount;
static u16 FreedBlock;

extern "C" {
CARDControl __CARDBlock[2];

s32 __CARDGetControlBlock(s32 channel, CARDControl** card)
{
	if (channel < 0 || channel >= CARD_NUM_CHANS) {
		return CARD_RESULT_FATAL_ERROR;
	}
	if (ControlResult < CARD_RESULT_READY) {
		return ControlResult;
	}
	*card = &__CARDBlock[channel];
	__CARDBlock[channel].result = CARD_RESULT_BUSY;
	return CARD_RESULT_READY;
}

s32 __CARDPutControlBlock(CARDControl* card, s32 result)
{
	++PutCount;
	card->result = result;
	return result;
}

CARDDirectoryBlock* __CARDGetDirBlock(CARDControl*)
{
	return &Directory;
}

s32 __CARDAccess(CARDControl*, CARDDir*)
{
	return AccessResult;
}

s32 __CARDIsPublic(CARDDir*)
{
	return PublicResult;
}

s32 __CARDGetFileNo(CARDControl*, const char*, s32* fileNo)
{
	if (FileLookupResult >= CARD_RESULT_READY) {
		*fileNo = FileLookupNumber;
	}
	return FileLookupResult;
}

BOOL __CARDIsOpened(CARDControl*, s32)
{
	return FileOpened;
}

s32 __CARDUpdateDir(s32 channel, CARDCallback callback)
{
	++UpdateCount;
	if (UpdateResult >= CARD_RESULT_READY && callback != nullptr) {
		callback(channel, UpdateResult);
	}
	return UpdateResult;
}

s32 __CARDFreeBlock(s32 channel, u16 block, CARDCallback callback)
{
	++FreeCount;
	FreedBlock = block;
	if (FreeResult >= CARD_RESULT_READY && callback != nullptr) {
		callback(channel, FreeResult);
	}
	return FreeResult;
}

void __CARDDefaultApiCallback(s32, s32)
{
	++DefaultCallbackCount;
}

void __CARDSyncCallback(s32, s32)
{
	++SyncCallbackCount;
}

s32 __CARDSync(s32)
{
	++SyncCount;
	return SyncResult;
}
}

static void UserCallback(s32, s32)
{
	++UserCallbackCount;
}

static int Fail(const char* message)
{
	std::fprintf(stderr, "%s\n", message);
	return 1;
}

static void ResetState()
{
	std::memset(&Directory, 0xff, sizeof(Directory));
	std::memset(__CARDBlock, 0, sizeof(__CARDBlock));
	__CARDBlock[0].attached = TRUE;
	__CARDBlock[1].attached = TRUE;
	ControlResult = CARD_RESULT_READY;
	AccessResult = CARD_RESULT_READY;
	PublicResult = CARD_RESULT_NOPERM;
	FileLookupResult = CARD_RESULT_READY;
	FileLookupNumber = 3;
	UpdateResult = CARD_RESULT_READY;
	FreeResult = CARD_RESULT_READY;
	SyncResult = CARD_RESULT_READY;
	FileOpened = FALSE;
	PutCount = 0;
	UpdateCount = 0;
	FreeCount = 0;
	SyncCount = 0;
	UserCallbackCount = 0;
	DefaultCallbackCount = 0;
	SyncCallbackCount = 0;
	FreedBlock = 0xffff;
	__CARDPermMask = CARD_ATTR_PUBLIC | CARD_ATTR_NO_COPY | CARD_ATTR_NO_MOVE;
}

static bool IsErased(const CARDDir& entry)
{
	const u8* bytes = reinterpret_cast<const u8*>(&entry);
	for (size_t i = 0; i < sizeof(entry); ++i) {
		if (bytes[i] != 0xff) {
			return false;
		}
	}
	return true;
}

int main()
{
	CARDDir output;
	CARDDir desired;
	CARDDir* entry;
	s32 result;
	u8 attributes;

	ResetState();
	entry = &Directory.entries[7];
	std::memset(entry, 0, sizeof(*entry));
	std::memcpy(entry->gameName, "GAME", 4);
	std::memcpy(entry->company, "CP", 2);
	std::memcpy(entry->fileName, "wire.dat", 9);
	CARDWireWrite32(&entry->time, 0x12345678);
	CARDWireWrite32(&entry->iconAddr, 0x01020304);
	CARDWireWrite16(&entry->iconFormat, 0x1122);
	CARDWireWrite16(&entry->iconSpeed, 0x3344);
	CARDWireWrite16(&entry->startBlock, 0x5566);
	CARDWireWrite16(&entry->length, 0x7788);
	CARDWireWrite16(&entry->reserved_3A, 0x99aa);
	CARDWireWrite32(&entry->commentAddr, 0xbbccddee);
	AccessResult = CARD_RESULT_NOPERM;
	PublicResult = CARD_RESULT_READY;
	std::memset(&output, 0, sizeof(output));
	result = __CARDGetStatusEx(0, 7, &output);
	if (result != CARD_RESULT_READY || output.time != 0x12345678 ||
	    output.iconAddr != 0x01020304 || output.iconFormat != 0x1122 ||
	    output.iconSpeed != 0x3344 || output.startBlock != 0x5566 ||
	    output.length != 0x7788 || output.reserved_3A != 0x99aa ||
	    output.commentAddr != 0xbbccddee ||
	    std::memcmp(output.gameName, "GAME", 4) != 0 ||
	    std::memcmp(output.company, "CP", 2) != 0 ||
	    std::memcmp(output.fileName, "wire.dat", 9) != 0 ||
	    PutCount != 1) {
		return Fail("__CARDGetStatusEx did not decode a complete public directory entry");
	}

	ResetState();
	entry = &Directory.entries[7];
	std::memset(entry, 0x2a, sizeof(*entry));
	entry->gameName[0] = 'G';
	AccessResult = CARD_RESULT_NOPERM;
	PublicResult = CARD_RESULT_NOPERM;
	std::memset(&output, 0x5c, sizeof(output));
	desired = output;
	result = __CARDGetStatusEx(0, 7, &output);
	if (result != CARD_RESULT_NOPERM || std::memcmp(&output, &desired, sizeof(output)) != 0) {
		return Fail("__CARDGetStatusEx copied a directory entry without permission");
	}
	if (__CARDGetStatusEx(0, CARD_MAX_FILE, &output) != CARD_RESULT_FATAL_ERROR) {
		return Fail("__CARDGetStatusEx accepted an invalid file number");
	}

	ResetState();
	entry = &Directory.entries[FileLookupNumber];
	std::memset(entry, 0x11, sizeof(*entry));
	CARDWireWrite16(&entry->startBlock, 42);
	result = CARDDeleteAsync(0, "save.dat", UserCallback);
	if (result != CARD_RESULT_READY || !IsErased(*entry) || __CARDBlock[0].startBlock != 42 ||
	    FreedBlock != 42 || FreeCount != 1 || UserCallbackCount != 1) {
		return Fail("CARDDeleteAsync did not release the directory entry and data chain");
	}

	ResetState();
	entry = &Directory.entries[FileLookupNumber];
	std::memset(entry, 0x21, sizeof(*entry));
	CARDWireWrite16(&entry->startBlock, 8);
	result = CARDDeleteAsync(0, "default.dat", nullptr);
	if (result != CARD_RESULT_READY || DefaultCallbackCount != 1 || !IsErased(*entry)) {
		return Fail("CARDDeleteAsync did not use the default callback for a null callback");
	}

	ResetState();
	entry = &Directory.entries[FileLookupNumber];
	std::memset(entry, 0x33, sizeof(*entry));
	CARDWireWrite16(&entry->startBlock, 9);
	FileOpened = TRUE;
	result = CARDDeleteAsync(0, "open.dat", UserCallback);
	if (result != CARD_RESULT_BUSY || IsErased(*entry) || UpdateCount != 0 || FreeCount != 0) {
		return Fail("CARDDeleteAsync deleted an open file");
	}

	ResetState();
	entry = &Directory.entries[FileLookupNumber];
	std::memset(entry, 0x44, sizeof(*entry));
	CARDWireWrite16(&entry->startBlock, 12);
	result = CARDDelete(0, "sync.dat");
	if (result != CARD_RESULT_READY || SyncCallbackCount != 1 || SyncCount != 1 ||
	    !IsErased(*entry)) {
		return Fail("CARDDelete did not complete through the synchronous callback path");
	}

	ResetState();
	entry = &Directory.entries[4];
	std::memset(entry, 0, sizeof(*entry));
	std::memcpy(entry->gameName, "OLDG", 4);
	std::memcpy(entry->company, "OC", 2);
	std::memcpy(entry->fileName, "old.dat", 8);
	CARDWireWrite16(&entry->startBlock, 77);
	CARDWireWrite16(&entry->length, 13);
	std::memset(&desired, 0x7e, sizeof(desired));
	std::memcpy(desired.gameName, "NEWG", 4);
	std::memcpy(desired.company, "NC", 2);
	std::memcpy(desired.fileName, "new.dat", 8);
	desired.time = 1234;
	desired.bannerFormat = 5;
	desired.iconAddr = 0x180;
	desired.iconFormat = 0x1234;
	desired.iconSpeed = 0x4321;
	desired.permission = CARD_ATTR_PUBLIC | CARD_ATTR_NO_COPY;
	desired.copyTimes = 6;
	desired.commentAddr = 0x240;
	result = __CARDSetStatusExAsync(0, 4, &desired, UserCallback);
	if (result != CARD_RESULT_READY || UserCallbackCount != 1 || UpdateCount != 1 ||
	    CARDWireRead16(&entry->startBlock) != 77 || CARDWireRead16(&entry->length) != 13 ||
	    std::memcmp(entry->gameName, desired.gameName, sizeof(entry->gameName)) != 0 ||
	    std::memcmp(entry->company, desired.company, sizeof(entry->company)) != 0 ||
	    std::memcmp(entry->fileName, desired.fileName, CARD_FILENAME_MAX) != 0 ||
	    CARDWireRead32(&entry->time) != desired.time || entry->bannerFormat != desired.bannerFormat ||
	    CARDWireRead32(&entry->iconAddr) != desired.iconAddr ||
	    CARDWireRead16(&entry->iconFormat) != desired.iconFormat ||
	    CARDWireRead16(&entry->iconSpeed) != desired.iconSpeed ||
	    entry->permission != desired.permission || entry->copyTimes != desired.copyTimes ||
	    CARDWireRead32(&entry->commentAddr) != desired.commentAddr) {
		return Fail("__CARDSetStatusExAsync copied the wrong directory fields");
	}
	for (size_t i = 8; i < CARD_FILENAME_MAX; ++i) {
		if (desired.fileName[i] != 0) {
			return Fail("__CARDSetStatusExAsync did not normalize filename padding");
		}
	}

	ResetState();
	entry = &Directory.entries[4];
	std::memset(entry, 0, sizeof(*entry));
	std::memcpy(entry->gameName, "OLDG", 4);
	std::memcpy(entry->company, "OC", 2);
	std::memcpy(entry->fileName, "old.dat", 8);
	std::memset(&desired, 0, sizeof(desired));
	std::memcpy(desired.gameName, "NEWG", 4);
	std::memcpy(desired.company, "NC", 2);
	std::memcpy(desired.fileName, "company.dat", 12);
	desired.permission = CARD_ATTR_COMPANY;
	result = __CARDSetStatusExAsync(0, 4, &desired, UserCallback);
	if (result != CARD_RESULT_READY || desired.gameName[0] != 0 ||
	    std::memcmp(desired.company, "NC", 2) != 0 || entry->gameName[0] != 0 ||
	    std::memcmp(entry->company, "NC", 2) != 0) {
		return Fail("__CARDSetStatusExAsync did not normalize a company-wide identity");
	}

	ResetState();
	entry = &Directory.entries[4];
	std::memset(entry, 0, sizeof(*entry));
	std::memcpy(entry->gameName, "OLDG", 4);
	std::memcpy(entry->company, "OC", 2);
	std::memcpy(entry->fileName, "old.dat", 8);
	std::memset(&desired, 0, sizeof(desired));
	std::memcpy(desired.gameName, "NEWG", 4);
	std::memcpy(desired.company, "NC", 2);
	std::memcpy(desired.fileName, "global.dat", 11);
	desired.permission = CARD_ATTR_GLOBAL;
	result = __CARDSetStatusExAsync(0, 4, &desired, UserCallback);
	if (result != CARD_RESULT_READY || desired.gameName[0] != 0 ||
	    desired.company[0] != 0 || entry->gameName[0] != 0 || entry->company[0] != 0) {
		return Fail("__CARDSetStatusExAsync did not normalize a global identity");
	}

	ResetState();
	entry = &Directory.entries[4];
	std::memset(entry, 0, sizeof(*entry));
	std::memcpy(entry->gameName, "OLDG", 4);
	std::memcpy(entry->company, "OC", 2);
	std::memcpy(entry->fileName, "old.dat", 8);
	std::memset(&desired, 0, sizeof(desired));
	std::memcpy(desired.gameName, "NEWG", 4);
	std::memcpy(desired.company, "NC", 2);
	std::memcpy(desired.fileName, "new.dat", 8);
	Directory.entries[5] = desired;
	result = __CARDSetStatusExAsync(0, 4, &desired, UserCallback);
	if (result != CARD_RESULT_EXIST || UpdateCount != 0 || PutCount != 1) {
		return Fail("__CARDSetStatusExAsync did not reject a duplicate directory identity");
	}

	ResetState();
	entry = &Directory.entries[4];
	std::memset(entry, 0, sizeof(*entry));
	std::memcpy(entry->fileName, "attr.dat", 9);
	entry->permission = CARD_ATTR_PUBLIC | CARD_ATTR_NO_COPY;
	attributes = 0xa5;
	result = CARDGetAttributes(0, 4, &attributes);
	if (result != CARD_RESULT_READY ||
	    attributes != (CARD_ATTR_PUBLIC | CARD_ATTR_NO_COPY) ||
	    PutCount != 1) {
		return Fail("CARDGetAttributes did not return the directory permission byte");
	}

	ResetState();
	entry = &Directory.entries[4];
	std::memset(entry, 0, sizeof(*entry));
	std::memcpy(entry->fileName, "private.dat", 12);
	entry->permission = CARD_ATTR_NO_COPY;
	AccessResult = CARD_RESULT_NOPERM;
	PublicResult = CARD_RESULT_NOPERM;
	attributes = 0xa5;
	result = CARDGetAttributes(0, 4, &attributes);
	if (result != CARD_RESULT_NOPERM || attributes != 0xa5 || PutCount != 1) {
		return Fail("CARDGetAttributes changed output after a permission failure");
	}

	ResetState();
	entry = &Directory.entries[4];
	std::memset(entry, 0, sizeof(*entry));
	std::memcpy(entry->fileName, "setattr.dat", 12);
	entry->permission = CARD_ATTR_PUBLIC;
	result = CARDSetAttributesAsync(
	    0,
	    4,
	    CARD_ATTR_NO_COPY | CARD_ATTR_NO_MOVE,
	    UserCallback);
	if (result != CARD_RESULT_READY ||
	    entry->permission != (CARD_ATTR_NO_COPY | CARD_ATTR_NO_MOVE) ||
	    PutCount != 1 || UpdateCount != 1 || UserCallbackCount != 1) {
		return Fail("CARDSetAttributesAsync did not update through CARDStatEx");
	}

	ResetState();
	result = CARDSetAttributesAsync(0, 4, 0x80, UserCallback);
	if (result != CARD_RESULT_NOPERM || PutCount != 0 || UpdateCount != 0 ||
	    UserCallbackCount != 0) {
		return Fail("CARDSetAttributesAsync accepted an attribute outside the permission mask");
	}

	ResetState();
	__CARDPermMask |= CARD_ATTR_GLOBAL | CARD_ATTR_COMPANY;
	entry = &Directory.entries[4];
	std::memset(entry, 0, sizeof(*entry));
	std::memcpy(entry->fileName, "global.dat", 11);
	entry->permission = CARD_ATTR_GLOBAL;
	result = CARDSetAttributesAsync(0, 4, CARD_ATTR_PUBLIC, UserCallback);
	if (result != CARD_RESULT_NOPERM || PutCount != 1 || UpdateCount != 0 ||
	    UserCallbackCount != 0) {
		return Fail("CARDSetAttributesAsync allowed GLOBAL scope to be removed");
	}

	ResetState();
	__CARDPermMask |= CARD_ATTR_GLOBAL | CARD_ATTR_COMPANY;
	entry = &Directory.entries[4];
	std::memset(entry, 0, sizeof(*entry));
	std::memcpy(entry->fileName, "company.dat", 12);
	entry->permission = CARD_ATTR_COMPANY;
	result = CARDSetAttributesAsync(
	    0,
	    4,
	    CARD_ATTR_GLOBAL | CARD_ATTR_COMPANY,
	    UserCallback);
	if (result != CARD_RESULT_NOPERM || PutCount != 1 || UpdateCount != 0 ||
	    UserCallbackCount != 0) {
		return Fail("CARDSetAttributesAsync allowed incompatible file scopes");
	}

	ResetState();
	entry = &Directory.entries[4];
	std::memset(entry, 0, sizeof(*entry));
	std::memcpy(entry->fileName, "syncattr.dat", 13);
	entry->permission = CARD_ATTR_PUBLIC;
	result = CARDSetAttributes(0, 4, CARD_ATTR_NO_MOVE);
	if (result != CARD_RESULT_READY || entry->permission != CARD_ATTR_NO_MOVE ||
	    UpdateCount != 1 || SyncCallbackCount != 1 || SyncCount != 1) {
		return Fail("CARDSetAttributes did not complete through the synchronous path");
	}

	return 0;
}
