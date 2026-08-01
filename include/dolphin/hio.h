#ifndef _DOLPHIN_HIO_H
#define _DOLPHIN_HIO_H

#include <dolphin/types.h>

BEGIN_SCOPE_EXTERN_C

#define HIO_STATUS_TX 0x00000001
#define HIO_STATUS_RX 0x00000002
#define HIO_STATUS_ID 0x00000004

#define HIO_NOTIFY_INIT_REQUEST 0x0001
#define HIO_NOTIFY_EXIT_DONE    0x0002

typedef void (*HIOCallback)(void);
typedef BOOL (*HIOEnumCallback)(s32 chan);
typedef s32 (*HIONotify)(s32 event, void* param);

BOOL HIOEnumDevices(HIOEnumCallback callback);
BOOL HIOInit(s32 chan, HIOCallback callback);
BOOL HIOInitEx(s32 chan, u32 dev, HIOCallback callback);
BOOL HIOReadMailbox(u32* word);
BOOL HIOWriteMailbox(u32 word);
BOOL HIORead(u32 addr, void* buffer, s32 size);
BOOL HIOWrite(u32 addr, const void* buffer, s32 size);
BOOL HIOReadAsync(u32 addr, void* buffer, s32 size, HIOCallback callback);
BOOL HIOWriteAsync(u32 addr, const void* buffer, s32 size,
                   HIOCallback callback);
BOOL HIOReadStatus(u32* status);
BOOL HIOInit2(s32 chan, HIOCallback callback, HIONotify notify, void* param);
BOOL HIOInitEx2(s32 chan, s32 dev, HIOCallback callback, HIONotify notify,
                void* param);

#if defined(_WIN32) || defined(WIN32)
void HIOExit(void);
#endif

END_SCOPE_EXTERN_C

#endif /* _DOLPHIN_HIO_H */
