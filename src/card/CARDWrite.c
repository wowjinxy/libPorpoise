/*---------------------------------------------------------------------------*
  CARDWrite.c - File Write Operations
 *---------------------------------------------------------------------------*/

#include <dolphin/card.h>
#include <dolphin/card_internal.h>
#include <dolphin/os.h>

/*---------------------------------------------------------------------------*
  Name:         CARDWriteAsync

  Description:  Write to file (asynchronous).

  Arguments:    fileInfo  File info
                buf       Source buffer
                length    Bytes to write
                offset    File offset
                callback  Completion callback

  Returns:      Bytes written, or error
 *---------------------------------------------------------------------------*/
s32 CARDWriteAsync(CARDFileInfo* fileInfo, const void* buf, s32 length,
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
    
    // Stub - return success
    if (callback) {
        callback(chan, length);
    }
    
    return length;
}

/*---------------------------------------------------------------------------*
  Name:         CARDWrite

  Description:  Write to file (synchronous).

  Arguments:    fileInfo  File info
                buf       Source buffer
                length    Bytes to write
                offset    File offset

  Returns:      Bytes written, or error
 *---------------------------------------------------------------------------*/
s32 CARDWrite(CARDFileInfo* fileInfo, const void* buf, s32 length, s32 offset) {
    return CARDWriteAsync(fileInfo, buf, length, offset, NULL);
}

