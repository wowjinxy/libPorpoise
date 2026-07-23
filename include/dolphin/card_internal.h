/**
 * @file card_internal.h
 * @brief Internal CARD module definitions (shared between CARD source files)
 */

#ifndef DOLPHIN_CARD_INTERNAL_H
#define DOLPHIN_CARD_INTERNAL_H

#include <dolphin/card.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*
    Internal State Structure
 *---------------------------------------------------------------------------*/

typedef struct CARDState {
    BOOL        mounted;
    BOOL        formatted;
    s32         lastResult;
    void*       workArea;
    CARDCallback detachCallback;
    DVDDiskID   diskID;
    char        openFiles[127][CARD_FILENAME_MAX];  // Track open files by fileNo
} CARDState;

/*---------------------------------------------------------------------------*
    Shared State (defined in CARDBios.c)
 *---------------------------------------------------------------------------*/

extern CARDState __CARDCards[CARD_MAX_CHAN];
extern const char* __CARDCardPaths[CARD_MAX_CHAN];
extern BOOL __CARDInitialized;

/*---------------------------------------------------------------------------*
    Internal Helper Functions
 *---------------------------------------------------------------------------*/

void __CARDBuildFilePath(s32 chan, const char* fileName, char* outPath, size_t maxLen);

// Low-level operations
s32 __CARDEraseSector(s32 chan, u32 addr, CARDCallback callback);
void __CARDCheckSum(void* ptr, int length, u16* checkSum, u16* checkSumInv);
s32 __CARDReadSegment(s32 chan, CARDCallback callback);
s32 __CARDWritePage(s32 chan, CARDCallback callback);

#ifdef __cplusplus
}
#endif

#endif // DOLPHIN_CARD_INTERNAL_H

