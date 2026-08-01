#include <dolphin/card.h>
#include <stddef.h>
#include <string.h>

#include "CARDWire.h"

static void WriteCallback(s32 channel, s32 result);
static void EraseCallback(s32 channel, s32 result);

/**
 * @TODO: Documentation
 */
CARDFatBlock* __CARDGetFatBlock(CARDControl* card)
{
	return card->currentFat;
}

/**
 * @TODO: Documentation
 */
static void WriteCallback(s32 channel, s32 result)
{
	CARDControl* card;
	CARDCallback callback;
	CARDFatBlock* fat;
	CARDFatBlock* fatBack;

	card = &__CARDBlock[channel];

	if (result >= CARD_RESULT_READY) {
		fat     = &card->workArea->blockAllocMap;
		fatBack = &card->workArea->blockAllocMapBackup;

		if (card->currentFat == fat) {
			card->currentFat = fatBack;
			memcpy(fatBack, fat, 0x2000);
		} else {
			card->currentFat = fat;
			memcpy(fat, fatBack, 0x2000);
		}
	}

	if (card->apiCallback == NULL) {
		__CARDPutControlBlock(card, result);
	}

	callback = card->eraseCallback;
	if (callback) {
		card->eraseCallback = NULL;
		callback(channel, result);
	}
}

/**
 * @TODO: Documentation
 */
static void EraseCallback(s32 channel, s32 result)
{
	CARDControl* card;
	CARDCallback callback;
	CARDFatBlock* fat;
	u32 addr;
	STACK_PAD_VAR(2); /* this compiler sucks */

	card = &__CARDBlock[channel];
	if (result < CARD_RESULT_READY) {
		goto error;
	}

	fat    = __CARDGetFatBlock(card);
	addr   = ((u32)fat - (u32)card->workArea) / CARD_SYSTEM_BLOCK_SIZE * card->sectorSize;
	result = __CARDWrite(channel, addr, CARD_SYSTEM_BLOCK_SIZE, fat, WriteCallback);
	if (result < CARD_RESULT_READY) {
		goto error;
	}

	return;

error:
	if (card->apiCallback == NULL) {
		__CARDPutControlBlock(card, result);
	}
	callback = card->eraseCallback;
	if (callback) {
		card->eraseCallback = NULL;
		callback(channel, result);
	}
}

/**
 * @TODO: Documentation
 */
s32 __CARDAllocBlock(s32 chan, u32 cBlock, CARDCallback callback)
{
	CARDControl* card;
	CARDFatBlock* fat;
	u16 iBlock;
	u16 startBlock;
	u16 prevBlock;
	u16 count;
	u16 freeBlocks;

	card = &__CARDBlock[chan];
	if (!card->attached) {
		return CARD_RESULT_NOCARD;
	}

	fat = __CARDGetFatBlock(card);
	freeBlocks = CARDWireRead16(&fat->freeBlocks);
	if (freeBlocks < cBlock) {
		return CARD_RESULT_INSSPACE;
	}

	CARDWireWrite16(&fat->freeBlocks, (u16)(freeBlocks - cBlock));
	startBlock = 0xFFFF;
	iBlock     = CARDWireRead16(&fat->lastAllocBlock);
	count      = 0;
	while (0 < cBlock) {
		if (card->cBlock - 5 < ++count) {
			return CARD_RESULT_BROKEN;
		}

		iBlock++;
		if (!CARDIsValidBlockNo(card, iBlock)) {
			iBlock = 5;
		}

		if (CARDWireRead16((u8*)fat + iBlock * sizeof(u16)) == CARD_FAT_AVAIL) {
			if (startBlock == 0xFFFF) {
				startBlock = iBlock;
			} else {
				CARDWireWrite16((u8*)fat + prevBlock * sizeof(u16), iBlock);
			}
			prevBlock           = iBlock;
			CARDWireWrite16((u8*)fat + iBlock * sizeof(u16), 0xFFFF);
			--cBlock;
		}
	}
	CARDWireWrite16(&fat->lastAllocBlock, iBlock);
	card->startBlock    = startBlock;

	return __CARDUpdateFatBlock(chan, fat, callback);
}

/**
 * @TODO: Documentation
 */
s32 __CARDFreeBlock(s32 channel, u16 nBlock, CARDCallback callback)
{
	CARDControl* card;
	CARDFatBlock* fat;
	u16 nextBlock;
	u16 freeBlocks;

	card = &__CARDBlock[channel];
	if (!card->attached) {
		return CARD_RESULT_NOCARD;
	}

	fat = __CARDGetFatBlock(card);
	freeBlocks = CARDWireRead16(&fat->freeBlocks);
	while (nBlock != 0xFFFF) {
		if (!CARDIsValidBlockNo(card, nBlock)) {
			return CARD_RESULT_BROKEN;
		}

		nextBlock = CARDWireRead16((u8*)fat + nBlock * sizeof(u16));
		CARDWireWrite16((u8*)fat + nBlock * sizeof(u16), CARD_FAT_AVAIL);
		nBlock      = nextBlock;
		freeBlocks++;
	}
	CARDWireWrite16(&fat->freeBlocks, freeBlocks);

	return __CARDUpdateFatBlock(channel, fat, callback);
}

/**
 * @TODO: Documentation
 */
s32 __CARDUpdateFatBlock(s32 channel, CARDFatBlock* fat, CARDCallback callback)
{
	CARDControl* card;
	u16 checkSum;
	u16 checkSumInv;

	card = &__CARDBlock[channel];
	CARDWireWrite16(&fat->checkCode, (u16)(CARDWireRead16(&fat->checkCode) + 1));
	__CARDCheckSum(&fat->checkCode, 0x1FFC, &checkSum, &checkSumInv);
	CARDWireWrite16(&fat->checkSum, checkSum);
	CARDWireWrite16(&fat->checkSumInv, checkSumInv);
	DCStoreRange(fat, 0x2000);
	card->eraseCallback = callback;

	return __CARDEraseSector(channel, (((u32)fat - (u32)card->workArea) / CARD_SYSTEM_BLOCK_SIZE) * card->sectorSize, EraseCallback);
}
