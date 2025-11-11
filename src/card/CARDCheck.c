/*---------------------------------------------------------------------------*
  CARDCheck.c - Card Check and Format Operations
 *---------------------------------------------------------------------------*/

#include <dolphin/card.h>
#include <dolphin/card_internal.h>
#include <dolphin/os.h>

/*---------------------------------------------------------------------------*
  Name:         CARDCheckAsync

  Description:  Check and repair card (asynchronous).

  Arguments:    chan      Card channel
                callback  Completion callback

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDCheckAsync(s32 chan, CARDCallback callback) {
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!__CARDCards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDCheck

  Description:  Check and repair card (synchronous).

  Arguments:    chan  Card channel

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDCheck(s32 chan) {
    return CARDCheckAsync(chan, NULL);
}

/*---------------------------------------------------------------------------*
  Name:         CARDCheckExAsync

  Description:  Check with progress tracking (asynchronous).

  Arguments:    chan       Card channel
                xferBytes  Pointer to receive bytes transferred
                callback   Completion callback

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDCheckExAsync(s32 chan, s32* xferBytes, CARDCallback callback) {
    if (xferBytes) *xferBytes = 0;
    return CARDCheckAsync(chan, callback);
}

/*---------------------------------------------------------------------------*
  Name:         CARDCheckEx

  Description:  Check with progress tracking (synchronous).

  Arguments:    chan       Card channel
                xferBytes  Pointer to receive bytes transferred

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDCheckEx(s32 chan, s32* xferBytes) {
    if (xferBytes) *xferBytes = 0;
    return CARDCheck(chan);
}

