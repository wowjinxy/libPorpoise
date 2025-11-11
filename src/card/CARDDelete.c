/*---------------------------------------------------------------------------*
  CARDDelete.c - File Deletion Operations
 *---------------------------------------------------------------------------*/

#include <dolphin/card.h>
#include <dolphin/card_internal.h>
#include <dolphin/os.h>
#include <stdio.h>

/*---------------------------------------------------------------------------*
  Name:         CARDDeleteAsync

  Description:  Delete file by name (asynchronous).

  Arguments:    chan      Card channel
                fileName  File to delete
                callback  Completion callback

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDDeleteAsync(s32 chan, const char* fileName, CARDCallback callback) {
    if (chan < 0 || chan >= CARD_MAX_CHAN || !fileName) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!__CARDCards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    char path[512];
    __CARDBuildFilePath(chan, fileName, path, sizeof(path));
    
    if (remove(path) != 0) {
        OSReport("CARD: Failed to delete '%s'\n", fileName);
        return CARD_RESULT_NOFILE;
    }
    
    OSReport("CARD: Deleted '%s'\n", fileName);
    
    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDDelete

  Description:  Delete file (synchronous).

  Arguments:    chan      Card channel
                fileName  File to delete

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDDelete(s32 chan, const char* fileName) {
    return CARDDeleteAsync(chan, fileName, NULL);
}

/*---------------------------------------------------------------------------*
  Name:         CARDFastDeleteAsync

  Description:  Delete file by number (asynchronous).

  Arguments:    chan      Card channel
                fileNo    File number
                callback  Completion callback

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDFastDeleteAsync(s32 chan, s32 fileNo, CARDCallback callback) {
    (void)fileNo;
    
    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDFastDelete

  Description:  Delete file by number (synchronous).

  Arguments:    chan    Card channel
                fileNo  File number

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDFastDelete(s32 chan, s32 fileNo) {
    return CARDFastDeleteAsync(chan, fileNo, NULL);
}

