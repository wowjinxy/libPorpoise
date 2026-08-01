#include <dolphin/card.h>
#include <string.h>

#include "CARDWire.h"

/**
 * @TODO: Documentation
 */
static void UpdateIconOffsets(CARDStat* state)
{
	u32 offset;
	BOOL iconTlut;
	int i;

	offset = state->iconAddr;
	if (offset == 0xffffffff) {
		state->bannerFormat = 0;
		state->iconFormat   = 0;
		state->iconSpeed    = 0;
		offset              = 0;
	}

	iconTlut = FALSE;
	switch (CARDGetBannerFormat(state)) {
	case CARD_STAT_BANNER_C8:
	{
		state->offsetBanner = offset;
		offset += CARD_BANNER_WIDTH * CARD_BANNER_HEIGHT;
		state->offsetBannerTlut = offset;
		offset += 2 * 256;
		break;
	}
	case CARD_STAT_BANNER_RGB5A3:
	{
		state->offsetBanner = offset;
		offset += 2 * CARD_BANNER_WIDTH * CARD_BANNER_HEIGHT;
		state->offsetBannerTlut = 0xffffffff;
		break;
	}
	default:
	{
		state->offsetBanner     = 0xffffffff;
		state->offsetBannerTlut = 0xffffffff;
		break;
	}
	}
	for (i = 0; i < CARD_ICON_MAX; ++i) {
		switch (CARDGetIconFormat(state, i)) {
		case CARD_STAT_ICON_C8:
		{
			state->offsetIcon[i] = offset;
			offset += CARD_ICON_WIDTH * CARD_ICON_HEIGHT;
			iconTlut = TRUE;
			break;
		}
		case CARD_STAT_ICON_RGB5A3:
		{
			state->offsetIcon[i] = offset;
			offset += 2 * CARD_ICON_WIDTH * CARD_ICON_HEIGHT;
			break;
		}
		default:
		{
			state->offsetIcon[i] = 0xffffffff;
			break;
		}
		}
	}
	if (iconTlut) {
		state->offsetIconTlut = offset;
		offset += 2 * 256;
	} else {
		state->offsetIconTlut = 0xffffffff;
	}
	state->offsetData = offset;
}

/**
 * @TODO: Documentation
 */
s32 CARDGetStatus(s32 channel, s32 fileNo, CARDStat* state)
{
	CARDControl* card;
	CARDDirectoryBlock* dir;
	CARDDir* ent;
	s32 result;

	if (fileNo < 0 || CARD_MAX_FILE <= fileNo) {
		return CARD_RESULT_FATAL_ERROR;
	}
	result = __CARDGetControlBlock(channel, &card);
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
		memcpy(state->gameName, ent->gameName, sizeof(state->gameName));
		memcpy(state->company, ent->company, sizeof(state->company));
		state->length = (u32)CARDWireRead16(&ent->length) * card->sectorSize;
		memcpy(state->fileName, ent->fileName, CARD_FILENAME_MAX);
		state->time = CARDWireRead32(&ent->time);

		state->bannerFormat = ent->bannerFormat;
		state->iconAddr     = CARDWireRead32(&ent->iconAddr);
		state->iconFormat   = CARDWireRead16(&ent->iconFormat);
		state->iconSpeed    = CARDWireRead16(&ent->iconSpeed);
		state->commentAddr  = CARDWireRead32(&ent->commentAddr);

		UpdateIconOffsets(state);
	}
	return __CARDPutControlBlock(card, result);
}

/**
 * @TODO: Documentation
 */
s32 CARDSetStatusAsync(s32 channel, s32 fileNo, CARDStat* state, CARDCallback callback)
{
	CARDControl* card;
	CARDDirectoryBlock* dir;
	CARDDir* ent;
	s32 result;

	if (fileNo < 0 || CARD_MAX_FILE <= fileNo || (state->iconAddr != 0xffffffff && CARD_READ_SIZE <= state->iconAddr)
	    || (state->commentAddr != 0xffffffff && CARD_SYSTEM_BLOCK_SIZE - CARD_COMMENT_SIZE < state->commentAddr % CARD_SYSTEM_BLOCK_SIZE)) {
		return CARD_RESULT_FATAL_ERROR;
	}
	result = __CARDGetControlBlock(channel, &card);
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

	ent->bannerFormat = state->bannerFormat;
	CARDWireWrite32(&ent->iconAddr, state->iconAddr);
	CARDWireWrite16(&ent->iconFormat, state->iconFormat);
	CARDWireWrite16(&ent->iconSpeed, state->iconSpeed);
	CARDWireWrite32(&ent->commentAddr, state->commentAddr);
	UpdateIconOffsets(state);

	if (CARDWireRead32(&ent->iconAddr) == 0xffffffff) {
		u16 iconSpeed = CARDWireRead16(&ent->iconSpeed);
		iconSpeed      = (u16)((iconSpeed & ~CARD_STAT_SPEED_MASK) | CARD_STAT_SPEED_FAST);
		CARDWireWrite16(&ent->iconSpeed, iconSpeed);
	}

	CARDWireWrite32(&ent->time, (u32)OSTicksToSeconds(OSGetTime()));
	result    = __CARDUpdateDir(channel, callback);
	if (result < CARD_RESULT_READY) {
		__CARDPutControlBlock(card, result);
	}
	return result;
}

/**
 * @TODO: Documentation
 */
s32 CARDSetStatus(s32 channel, s32 fileNo, CARDStat* state)
{
	s32 result = CARDSetStatusAsync(channel, fileNo, state, __CARDSyncCallback);
	if (result < CARD_RESULT_READY) {
		return result;
	}

	return __CARDSync(channel);
}
