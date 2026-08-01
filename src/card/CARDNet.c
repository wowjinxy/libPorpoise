#include <dolphin/card.h>

u16 __CARDVendorID = 0xffff;
u8 __CARDPermMask =
	CARD_ATTR_PUBLIC | CARD_ATTR_NO_COPY | CARD_ATTR_NO_MOVE;

s32 CARDGetAttributes(s32 chan, s32 fileNo, u8* attr)
{
	CARDDir dirent;
	s32 result;

	result = __CARDGetStatusEx(chan, fileNo, &dirent);
	if (result == CARD_RESULT_READY) {
		*attr = dirent.permission;
	}
	return result;
}

s32 CARDSetAttributesAsync(
	s32 chan,
	s32 fileNo,
	u8 attr,
	CARDCallback callback)
{
	CARDDir dirent;
	s32 result;

	if (attr & (u8)~__CARDPermMask) {
		return CARD_RESULT_NOPERM;
	}

	result = __CARDGetStatusEx(chan, fileNo, &dirent);
	if (result < CARD_RESULT_READY) {
		return result;
	}

	/* Global/company scope cannot be removed or combined after creation. */
	if (((dirent.permission & CARD_ATTR_GLOBAL) &&
	     !(attr & CARD_ATTR_GLOBAL)) ||
	    ((dirent.permission & CARD_ATTR_COMPANY) &&
	     !(attr & CARD_ATTR_COMPANY)) ||
	    ((attr & CARD_ATTR_GLOBAL) &&
	     (attr & CARD_ATTR_COMPANY)) ||
	    ((attr & CARD_ATTR_GLOBAL) &&
	     (dirent.permission & CARD_ATTR_COMPANY)) ||
	    ((attr & CARD_ATTR_COMPANY) &&
	     (dirent.permission & CARD_ATTR_GLOBAL))) {
		return CARD_RESULT_NOPERM;
	}

	dirent.permission = attr;
	return __CARDSetStatusExAsync(chan, fileNo, &dirent, callback);
}

s32 CARDSetAttributes(s32 chan, s32 fileNo, u8 attr)
{
	s32 result;

	result = CARDSetAttributesAsync(
	    chan,
	    fileNo,
	    attr,
	    __CARDSyncCallback);
	if (result < CARD_RESULT_READY) {
		return result;
	}
	return __CARDSync(chan);
}
