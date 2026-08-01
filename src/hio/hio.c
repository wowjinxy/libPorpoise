#include <dolphin/hio.h>

/*
 * HIO is an optional development interface rather than part of the retail
 * console hardware.  A host without an attached HIO device reports that
 * absence synchronously.  Failed operations do not complete transfers,
 * invoke callbacks, or modify caller-owned storage.
 */

BOOL HIOEnumDevices(HIOEnumCallback callback)
{
	/* Enumeration itself succeeded even though this backend has no devices. */
	return callback != NULL;
}

BOOL HIOInit(s32 chan, HIOCallback callback)
{
	(void)chan;
	(void)callback;
	return FALSE;
}

BOOL HIOInitEx(s32 chan, u32 dev, HIOCallback callback)
{
	(void)chan;
	(void)dev;
	(void)callback;
	return FALSE;
}

BOOL HIOReadMailbox(u32* word)
{
	(void)word;
	return FALSE;
}

BOOL HIOWriteMailbox(u32 word)
{
	(void)word;
	return FALSE;
}

BOOL HIORead(u32 addr, void* buffer, s32 size)
{
	(void)addr;
	(void)buffer;
	(void)size;
	return FALSE;
}

BOOL HIOWrite(u32 addr, const void* buffer, s32 size)
{
	(void)addr;
	(void)buffer;
	(void)size;
	return FALSE;
}

BOOL HIOReadAsync(u32 addr, void* buffer, s32 size, HIOCallback callback)
{
	(void)addr;
	(void)buffer;
	(void)size;
	(void)callback;
	return FALSE;
}

BOOL HIOWriteAsync(u32 addr, const void* buffer, s32 size, HIOCallback callback)
{
	(void)addr;
	(void)buffer;
	(void)size;
	(void)callback;
	return FALSE;
}

BOOL HIOReadStatus(u32* status)
{
	(void)status;
	return FALSE;
}

BOOL HIOInit2(s32 chan, HIOCallback callback, HIONotify notify, void* param)
{
	(void)chan;
	(void)callback;
	(void)notify;
	(void)param;
	return FALSE;
}

BOOL HIOInitEx2(s32 chan, s32 dev, HIOCallback callback, HIONotify notify,
                void* param)
{
	(void)chan;
	(void)dev;
	(void)callback;
	(void)notify;
	(void)param;
	return FALSE;
}

#if defined(_WIN32) || defined(WIN32)
void HIOExit(void)
{
}
#endif
