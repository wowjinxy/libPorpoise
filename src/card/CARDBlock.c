/*---------------------------------------------------------------------------*
  CARDBlock.c - Block-Level Operations (Internal)
 *---------------------------------------------------------------------------*/

#include <dolphin/card.h>
#include <dolphin/card_internal.h>
#include <dolphin/os.h>

/*---------------------------------------------------------------------------*
  Name:         __CARDEraseSector

  Description:  Erase a flash sector.

  Arguments:    chan      Card channel
                addr      Sector address
                callback  Completion callback

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 __CARDEraseSector(s32 chan, u32 addr, CARDCallback callback) {
    (void)addr;
    
    /* On PC, no flash sectors to erase.
     * Just call callback immediately.
     */
    
    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         __CARDCheckSum

  Description:  Calculate checksum for data block.

  Arguments:    ptr         Data pointer
                length      Data length
                checkSum    Pointer to receive checksum
                checkSumInv Pointer to receive inverted checksum

  Returns:      None
 *---------------------------------------------------------------------------*/
void __CARDCheckSum(void* ptr, int length, u16* checkSum, u16* checkSumInv) {
    u16* data = (u16*)ptr;
    u16 sum = 0;
    
    for (int i = 0; i < length / 2; i++) {
        sum += data[i];
    }
    
    if (checkSum) {
        *checkSum = sum;
    }
    if (checkSumInv) {
        *checkSumInv = ~sum;
    }
}

/*---------------------------------------------------------------------------*
  Name:         __CARDReadSegment

  Description:  Read 512-byte segment from card.

  Arguments:    chan      Card channel
                callback  Completion callback

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 __CARDReadSegment(s32 chan, CARDCallback callback) {
    /* On PC, this is handled by higher-level CARDRead().
     * Just call callback.
     */
    
    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         __CARDWritePage

  Description:  Write 128-byte page to card.

  Arguments:    chan      Card channel
                callback  Completion callback

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 __CARDWritePage(s32 chan, CARDCallback callback) {
    /* On PC, this is handled by higher-level CARDWrite().
     * Just call callback.
     */
    
    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    
    return CARD_RESULT_READY;
}

