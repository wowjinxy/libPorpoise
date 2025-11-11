/*---------------------------------------------------------------------------*
  CARDRead.c - File Read Operations
 *---------------------------------------------------------------------------*/

#include <dolphin/card.h>
#include <dolphin/card_internal.h>
#include <dolphin/os.h>
#include <string.h>

/*---------------------------------------------------------------------------*
  Name:         CARDReadAsync

  Description:  Read from file (asynchronous).

  Arguments:    fileInfo  File info
                buf       Destination buffer
                length    Bytes to read
                offset    File offset
                callback  Completion callback

  Returns:      Bytes read, or error
 *---------------------------------------------------------------------------*/
s32 CARDReadAsync(CARDFileInfo* fileInfo, void* buf, s32 length,
                  s32 offset, CARDCallback callback) {
    if (!fileInfo || !buf) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    s32 chan = fileInfo->chan;
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!__CARDCards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    // Stub - zero buffer
    memset(buf, 0, length);
    
    if (callback) {
        callback(chan, length);
    }
    
    return length;
}

/*---------------------------------------------------------------------------*
  Name:         CARDRead

  Description:  Read from file (synchronous).

  Arguments:    fileInfo  File info
                buf       Destination buffer
                length    Bytes to read
                offset    File offset

  Returns:      Bytes read, or error
 *---------------------------------------------------------------------------*/
s32 CARDRead(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset) {
    return CARDReadAsync(fileInfo, buf, length, offset, NULL);
}

