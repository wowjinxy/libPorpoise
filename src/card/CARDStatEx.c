#include <dolphin/card.h>

#include <string.h>

#include "CARDWire.h"

s32 __CARDGetStatusEx(s32 chan, s32 fileNo, CARDDir* dirent)
{
	CARDControl* card;
	CARDDirectoryBlock* dir;
	CARDDir* ent;
	s32 result;

	if (fileNo < 0 || CARD_MAX_FILE <= fileNo) {
		return CARD_RESULT_FATAL_ERROR;
	}
	result = __CARDGetControlBlock(chan, &card);
	if (result < CARD_RESULT_READY) {
		return result;
	}

	dir = __CARDGetDirBlock(card);
	ent = &dir->entries[fileNo];
#if OS_BUILD_VERSION >= 20011112L
	result = __CARDAccess(card, ent);
#else
	result = __CARDAccess(ent);
#endif
	if (result == CARD_RESULT_NOPERM) {
		result = __CARDIsPublic(ent);
	}
	if (result >= CARD_RESULT_READY) {
		memcpy(dirent, ent, sizeof(CARDDir));
		dirent->time       = CARDWireRead32(&ent->time);
		dirent->iconAddr   = CARDWireRead32(&ent->iconAddr);
		dirent->iconFormat = CARDWireRead16(&ent->iconFormat);
		dirent->iconSpeed  = CARDWireRead16(&ent->iconSpeed);
		dirent->startBlock = CARDWireRead16(&ent->startBlock);
		dirent->length     = CARDWireRead16(&ent->length);
		dirent->reserved_3A = CARDWireRead16(&ent->reserved_3A);
		dirent->commentAddr = CARDWireRead32(&ent->commentAddr);
	}
	return __CARDPutControlBlock(card, result);
}

s32 __CARDSetStatusExAsync(
	s32 chan, s32 fileNo, CARDDir* dirent, CARDCallback callback)
{
	CARDControl* card;
	CARDDirectoryBlock* dir;
	CARDDir* ent;
	s32 result;
	u8* cursor;
	s32 i;

	if (fileNo < 0 || CARD_MAX_FILE <= fileNo || dirent->fileName[0] == 0xff ||
	    dirent->fileName[0] == 0x00 ||
	    (dirent->iconAddr != 0xffffffff && CARD_READ_SIZE <= dirent->iconAddr) ||
	    (dirent->commentAddr != 0xffffffff &&
	     CARD_SYSTEM_BLOCK_SIZE - CARD_COMMENT_SIZE <
		 dirent->commentAddr % CARD_SYSTEM_BLOCK_SIZE)) {
		return CARD_RESULT_FATAL_ERROR;
	}

	result = __CARDGetControlBlock(chan, &card);
	if (result < CARD_RESULT_READY) {
		return result;
	}
	dir = __CARDGetDirBlock(card);
	ent = &dir->entries[fileNo];
#if OS_BUILD_VERSION >= 20011112L
	result = __CARDAccess(card, ent);
#else
	result = __CARDAccess(ent);
#endif
	if (result < CARD_RESULT_READY) {
		return __CARDPutControlBlock(card, result);
	}

	for (cursor = dirent->fileName;
	     cursor < &dirent->fileName[CARD_FILENAME_MAX];
	     ++cursor) {
		if (*cursor == 0x00) {
			while (++cursor < &dirent->fileName[CARD_FILENAME_MAX]) {
				*cursor = 0x00;
			}
			break;
		}
	}

	if (dirent->permission & CARD_ATTR_GLOBAL) {
		memset(dirent->gameName, 0, sizeof(dirent->gameName));
		memset(dirent->company, 0, sizeof(dirent->company));
	} else if (dirent->permission & CARD_ATTR_COMPANY) {
		memset(dirent->gameName, 0, sizeof(dirent->gameName));
	}

	if (memcmp(ent->fileName, dirent->fileName, CARD_FILENAME_MAX) != 0 ||
	    memcmp(ent->gameName, dirent->gameName, sizeof(ent->gameName)) != 0 ||
	    memcmp(ent->company, dirent->company, sizeof(ent->company)) != 0) {
		for (i = 0; i < CARD_MAX_FILE; ++i) {
			CARDDir* other;

			if (i == fileNo) {
				continue;
			}
			other = &dir->entries[i];
			if (other->gameName[0] == 0xff) {
				continue;
			}
			if (memcmp(other->gameName, dirent->gameName, sizeof(other->gameName)) == 0 &&
			    memcmp(other->company, dirent->company, sizeof(other->company)) == 0 &&
			    memcmp(other->fileName, dirent->fileName, CARD_FILENAME_MAX) == 0) {
				return __CARDPutControlBlock(card, CARD_RESULT_EXIST);
			}
		}

		memcpy(ent->fileName, dirent->fileName, CARD_FILENAME_MAX);
		memcpy(ent->gameName, dirent->gameName, sizeof(ent->gameName));
		memcpy(ent->company, dirent->company, sizeof(ent->company));
	}

	CARDWireWrite32(&ent->time, dirent->time);
	ent->bannerFormat = dirent->bannerFormat;
	CARDWireWrite32(&ent->iconAddr, dirent->iconAddr);
	CARDWireWrite16(&ent->iconFormat, dirent->iconFormat);
	CARDWireWrite16(&ent->iconSpeed, dirent->iconSpeed);
	CARDWireWrite32(&ent->commentAddr, dirent->commentAddr);
	ent->permission = dirent->permission;
	ent->copyTimes = dirent->copyTimes;

	result = __CARDUpdateDir(chan, callback);
	if (result < CARD_RESULT_READY) {
		__CARDPutControlBlock(card, result);
	}
	return result;
}

s32 __CARDSetStatusEx(s32 chan, s32 fileNo, CARDDir* dirent)
{
	s32 result =
		__CARDSetStatusExAsync(chan, fileNo, dirent, __CARDSyncCallback);

	if (result < CARD_RESULT_READY) {
		return result;
	}
	return __CARDSync(chan);
}
