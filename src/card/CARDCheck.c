#include <dolphin/card.h>
#include <stddef.h>
#include <string.h>

#include "CARDWire.h"

/**
 * @TODO: Documentation
 */
void __CARDCheckSum(void* data, int length, u16* checksum, u16* checksumInv)
{
	CARDWireCalculateChecksum(data, length, checksum, checksumInv);
}

/**
 * @TODO: Documentation
 */
static s32 VerifyID(CARDControl* card)
{
	CARDID* id;
	u16 checksum;
	u16 checksumInv;
	OSSramEx* sramEx;
	OSTime rand;
	int i;

	id = &card->workArea->header.id;

	if (CARDWireRead16(&id->deviceID) != 0 || CARDWireRead16(&id->size) != card->size) {
		return CARD_RESULT_BROKEN;
	}

	__CARDCheckSum(id, sizeof(CARDID) - sizeof(u32), &checksum, &checksumInv);
	if (CARDWireRead16(&id->checkSum) != checksum || CARDWireRead16(&id->checkSumInv) != checksumInv) {
		return CARD_RESULT_BROKEN;
	}

	if (CARDWireRead16(&id->encode) != OSGetFontEncode()) {
		return CARD_RESULT_ENCODING;
	}

	rand   = (OSTime)CARDWireRead64(&id->serial[12]);
	sramEx = __OSLockSramEx();

	for (i = 0; i < 12; i++) {
		rand = (rand * 1103515245 + 12345) >> 16;
		if (id->serial[i] != (u8)(sramEx->flashID[card - __CARDBlock][i] + rand)) {
			__OSUnlockSramEx(FALSE);
			return CARD_RESULT_BROKEN;
		}
		rand = ((rand * 1103515245 + 12345) >> 16) & 0x7FFF;
	}

	__OSUnlockSramEx(FALSE);

	return CARD_RESULT_READY;
}

/**
 * @TODO: Documentation
 */
static s32 VerifyDir(CARDControl* card, int* outCurrent)
{
	CARDDirectoryBlock* dir[2];
	CARDDirCheck* check[2];
	u16 checkSum;
	u16 checkSumInv;
	int i;
	int errors;
	int current;

	current = errors = 0;
	for (i = 0; i < 2; i++) {
		dir[i]   = CARDGetDirectoryBlock(card, i);
		check[i] = &dir[i]->check;
		__CARDCheckSum(dir[i], CARD_SYSTEM_BLOCK_SIZE - sizeof(u32), &checkSum, &checkSumInv);
		if (CARDWireRead16(&check[i]->checkSum) != checkSum || CARDWireRead16(&check[i]->checkSumInv) != checkSumInv) {
			++errors;
			current          = i;
			card->currentDir = NULL;
		}
	}

	if (errors == 0) {
		if (card->currentDir == 0) {
			if ((CARDWireReadS16(&check[0]->checkCode) - CARDWireReadS16(&check[1]->checkCode)) < 0) {
				current = 0;
			} else {
				current = 1;
			}
			card->currentDir = dir[current];
			memcpy(dir[current], dir[current ^ 1], CARD_SYSTEM_BLOCK_SIZE);
		} else {
			current = (card->currentDir == dir[0]) ? 0 : 1;
		}
	}

	if (outCurrent) {
		*outCurrent = current;
	}
	return errors;
}

/**
 * @TODO: Documentation
 */
static s32 VerifyFAT(CARDControl* card, int* outCurrent)
{
	CARDFatBlock* fat[2];
	CARDFatBlock* fatp;
	u16 nBlock;
	u16 cFree;
	int i;
	u16 checkSum;
	u16 checkSumInv;
	int errors;
	int current;

	current = errors = 0;
	for (i = 0; i < 2; i++) {
		fatp = fat[i] = CARDGetFatBlock(card, i);

		__CARDCheckSum(&fatp->checkCode, CARD_SYSTEM_BLOCK_SIZE - sizeof(u32), &checkSum, &checkSumInv);
		if (CARDWireRead16(&fatp->checkSum) != checkSum || CARDWireRead16(&fatp->checkSumInv) != checkSumInv) {
			++errors;
			current          = i;
			card->currentFat = NULL;
			continue;
		}

		cFree = 0;
		for (nBlock = CARD_NUM_SYSTEM_BLOCK; nBlock < card->cBlock; nBlock++) {
			if (CARDWireRead16((u8*)fatp + nBlock * sizeof(u16)) == CARD_FAT_AVAIL) {
				cFree++;
			}
		}
		if (cFree != CARDWireRead16(&fatp->freeBlocks)) {
			++errors;
			current          = i;
			card->currentFat = NULL;
			continue;
		}
	}

	if (0 == errors) {
		if (card->currentFat == 0) {
			if ((CARDWireReadS16(&fat[0]->checkCode) - CARDWireReadS16(&fat[1]->checkCode)) < 0) {
				current = 0;
			} else {
				current = 1;
			}
			card->currentFat = fat[current];
			memcpy(fat[current], fat[current ^ 1], CARD_SYSTEM_BLOCK_SIZE);
		} else {
			current = (card->currentFat == fat[0]) ? 0 : 1;
		}
	}
	if (outCurrent) {
		*outCurrent = current;
	}
	return errors;
}

/**
 * @TODO: Documentation
 */
s32 __CARDVerify(CARDControl* card)
{
	s32 result;
	int errors;

	result = VerifyID(card);
	if (result < CARD_RESULT_READY) {
		return result;
	}

	errors = VerifyDir(card, NULL);
	errors += VerifyFAT(card, NULL);
	switch (errors) {
	case 0:
	{
		return CARD_RESULT_READY;
	}
	case 1:
	{
		return CARD_RESULT_BROKEN;
	}
	default:
	{
		return CARD_RESULT_BROKEN;
	}
	}
}

/**
 * @TODO: Documentation
 */
s32 CARDCheckExAsync(s32 channel, s32* xferBytes, CARDCallback callback)
{
	CARDControl* card;
	CARDDirectoryBlock* dir[2];
	CARDFatBlock* fat[2];
	u16* map;
	s32 result;
	int errors;
	int currentFat;
	int currentDir;
	s32 fileNo;
	u16 iBlock;
	u16 cBlock;
	u16 cFree;
	BOOL updateFat    = FALSE;
	BOOL updateDir    = FALSE;
	BOOL updateOrphan = FALSE;

	if (xferBytes) {
		*xferBytes = 0;
	}

	result = __CARDGetControlBlock(channel, &card);
	if (result < CARD_RESULT_READY) {
		return result;
	}

	result = VerifyID(card);
	if (result < CARD_RESULT_READY) {
		return __CARDPutControlBlock(card, result);
	}

	errors = VerifyDir(card, &currentDir);
	errors += VerifyFAT(card, &currentFat);
	if (1 < errors) {
		return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
	}

	dir[0] = &card->workArea->dirBlock;
	dir[1] = &card->workArea->dirBlockBackup;
	fat[0] = &card->workArea->blockAllocMap;
	fat[1] = &card->workArea->blockAllocMapBackup;

	switch (errors) {
	case 0:
	{
		break;
	}
	case 1:
	{
		if (!card->currentDir) {
			card->currentDir = dir[currentDir];
			memcpy(dir[currentDir], dir[currentDir ^ 1], CARD_SYSTEM_BLOCK_SIZE);
			updateDir = TRUE;
		} else {
			card->currentFat = fat[currentFat];
			memcpy(fat[currentFat], fat[currentFat ^ 1], CARD_SYSTEM_BLOCK_SIZE);
			updateFat = TRUE;
		}
		break;
	}
	}

	map = (u16*)fat[currentFat ^ 1];
	memset(map, 0, CARD_SYSTEM_BLOCK_SIZE);

	for (fileNo = 0; fileNo < CARD_MAX_FILE; fileNo++) {
		CARDDir* ent;
		u16 length;

		ent = &card->currentDir->entries[fileNo];
		if (ent->gameName[0] == 0xff) {
			continue;
		}
		length = CARDWireRead16(&ent->length);

		for (iBlock = CARDWireRead16(&ent->startBlock), cBlock = 0; iBlock != 0xFFFF && cBlock < length;
		     iBlock = CARDWireRead16((u8*)card->currentFat + iBlock * sizeof(u16)), ++cBlock) {
			if (!CARDIsValidBlockNo(card, iBlock) || 1 < ++map[iBlock]) {
				return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
			}
		}
		if (cBlock != length || iBlock != 0xFFFF) {
			return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
		}
	}

	cFree = 0;
	for (iBlock = CARD_NUM_SYSTEM_BLOCK; iBlock < card->cBlock; iBlock++) {
		u16 nextBlock;

		nextBlock = CARDWireRead16((u8*)card->currentFat + iBlock * sizeof(u16));
		if (map[iBlock] == 0) {
			if (nextBlock != CARD_FAT_AVAIL) {
				CARDWireWrite16((u8*)card->currentFat + iBlock * sizeof(u16), CARD_FAT_AVAIL);
				updateOrphan                     = TRUE;
			}
			cFree++;
		} else if (!CARDIsValidBlockNo(card, nextBlock) && nextBlock != 0xFFFF) {
			return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
		}
	}
	if (cFree != CARDWireRead16(&card->currentFat->freeBlocks)) {
		CARDWireWrite16(&card->currentFat->freeBlocks, cFree);
		updateOrphan                 = TRUE;
	}
	if (updateOrphan) {
		u16 checkSum;
		u16 checkSumInv;

		__CARDCheckSum(&card->currentFat->checkCode, CARD_SYSTEM_BLOCK_SIZE - sizeof(u32), &checkSum, &checkSumInv);
		CARDWireWrite16(&card->currentFat->checkSum, checkSum);
		CARDWireWrite16(&card->currentFat->checkSumInv, checkSumInv);
	}

	memcpy(fat[currentFat ^ 1], fat[currentFat], CARD_SYSTEM_BLOCK_SIZE);

	if (updateDir) {
		if (xferBytes) {
			*xferBytes = CARD_SYSTEM_BLOCK_SIZE;
		}
		return __CARDUpdateDir(channel, callback);
	}

	if (updateFat | updateOrphan) {
		if (xferBytes) {
			*xferBytes = CARD_SYSTEM_BLOCK_SIZE;
		}
		return __CARDUpdateFatBlock(channel, card->currentFat, callback);
	}

	__CARDPutControlBlock(card, CARD_RESULT_READY);
	if (callback) {
		BOOL enabled = OSDisableInterrupts();
		callback(channel, CARD_RESULT_READY);
		OSRestoreInterrupts(enabled);
	}
	return CARD_RESULT_READY;
}

/**
 * @TODO: Documentation
 */
s32 CARDCheckAsync(s32 channel, CARDCallback callback)
{
	s32 bytes;
	return CARDCheckExAsync(channel, &bytes, callback);
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00005C
 */
void CARDCheckEx(void)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 */
s32 CARDCheck(s32 channel)
{
	s32 result;
	s32 xferBytes;

	result = CARDCheckExAsync(channel, &xferBytes, __CARDSyncCallback);

	if (result < CARD_RESULT_READY || &xferBytes == NULL) {
		return result;
	}

	return __CARDSync(channel);
}
