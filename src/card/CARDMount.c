/*---------------------------------------------------------------------------*
  CARDMount.c - Memory Card Mount/Unmount Operations
 *---------------------------------------------------------------------------*/

#include <dolphin/card.h>
#include <dolphin/card_internal.h>
#include <dolphin/os.h>

/*---------------------------------------------------------------------------*
  Name:         CARDMountAsync

  Description:  Mount memory card (asynchronous).

  Arguments:    chan              Card channel
                workArea          Work buffer
                detachCallback    Detach callback
                attachCallback    Attach callback

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback,
                   CARDCallback attachCallback) {
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!CARDProbe(chan)) {
        __CARDCards[chan].lastResult = CARD_RESULT_NOCARD;
        return CARD_RESULT_NOCARD;
    }
    
    __CARDCards[chan].mounted = TRUE;
    __CARDCards[chan].formatted = TRUE;
    __CARDCards[chan].workArea = workArea;
    __CARDCards[chan].detachCallback = detachCallback;
    __CARDCards[chan].lastResult = CARD_RESULT_READY;
    
    OSReport("CARD: Mounted slot %c (%s/)\n", 'A' + chan, __CARDCardPaths[chan]);
    
    if (attachCallback) {
        attachCallback(chan, CARD_RESULT_READY);
    }
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDMount

  Description:  Mount memory card (synchronous).

  Arguments:    chan            Card channel
                workArea        Work buffer
                detachCallback  Detach callback

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDMount(s32 chan, void* workArea, CARDCallback detachCallback) {
    return CARDMountAsync(chan, workArea, detachCallback, NULL);
}

/*---------------------------------------------------------------------------*
  Name:         CARDUnmount

  Description:  Unmount memory card.

  Arguments:    chan  Card channel

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDUnmount(s32 chan) {
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!__CARDCards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    __CARDCards[chan].mounted = FALSE;
    __CARDCards[chan].workArea = NULL;
    
    OSReport("CARD: Unmounted slot %c\n", 'A' + chan);
    
    return CARD_RESULT_READY;
}

