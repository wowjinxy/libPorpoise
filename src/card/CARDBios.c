/*---------------------------------------------------------------------------*
  CARDBios.c - Memory Card BIOS Functions
  
  Core initialization, mounting, probing, and utility functions.
 *---------------------------------------------------------------------------*/

#include <dolphin/card.h>
#include <dolphin/card_internal.h>
#include <dolphin/os.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define stat _stat
#else
#include <unistd.h>
#endif

/*---------------------------------------------------------------------------*
    Module State (shared with other CARD files via card_internal.h)
 *---------------------------------------------------------------------------*/

const char* __CARDCardPaths[CARD_MAX_CHAN] = {
    "memcard_a",
    "memcard_b"
};

CARDState __CARDCards[CARD_MAX_CHAN];
BOOL __CARDInitialized = FALSE;

/*---------------------------------------------------------------------------*
  Name:         __CARDBuildFilePath

  Description:  Build full path to save file.

  Arguments:    chan      Card channel
                fileName  Filename
                outPath   Output buffer
                maxLen    Buffer size

  Returns:      None
 *---------------------------------------------------------------------------*/
void __CARDBuildFilePath(s32 chan, const char* fileName, char* outPath, size_t maxLen) {
    snprintf(outPath, maxLen, "%s/%s.sav", __CARDCardPaths[chan], fileName);
}

/*---------------------------------------------------------------------------*
  Name:         CARDInit

  Description:  Initialize memory card subsystem.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void CARDInit(void) {
    if (__CARDInitialized) {
        return;
    }
    
    OSReport("CARD: Initializing memory card subsystem...\n");
    
    // Initialize card state
    for (int i = 0; i < CARD_MAX_CHAN; i++) {
        __CARDCards[i].mounted = FALSE;
        __CARDCards[i].formatted = FALSE;
        __CARDCards[i].lastResult = CARD_RESULT_READY;
        __CARDCards[i].workArea = NULL;
        __CARDCards[i].detachCallback = NULL;
        memset(&__CARDCards[i].diskID, 0, sizeof(DVDDiskID));
        memset(__CARDCards[i].openFiles, 0, sizeof(__CARDCards[i].openFiles));
        
        // Create card directory if it doesn't exist
        struct stat st;
        if (stat(__CARDCardPaths[i], &st) != 0) {
            mkdir(__CARDCardPaths[i], 0755);
            OSReport("CARD: Created directory: %s\n", __CARDCardPaths[i]);
        }
    }
    
    __CARDInitialized = TRUE;
    OSReport("CARD: Initialized\n");
    OSReport("CARD: Slot A → %s/\n", __CARDCardPaths[0]);
    OSReport("CARD: Slot B → %s/\n", __CARDCardPaths[1]);
}

/*---------------------------------------------------------------------------*
  Name:         CARDProbe

  Description:  Check if memory card is present.

  Arguments:    chan  Card channel

  Returns:      TRUE if card present
 *---------------------------------------------------------------------------*/
BOOL CARDProbe(s32 chan) {
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return FALSE;
    }
    
    struct stat st;
    return (stat(__CARDCardPaths[chan], &st) == 0);
}

/*---------------------------------------------------------------------------*
  Name:         CARDProbeEx

  Description:  Probe with extended information.

  Arguments:    chan        Card channel
                memSize     Pointer to receive memory size
                sectorSize  Pointer to receive sector size

  Returns:      Memory size code, or error
 *---------------------------------------------------------------------------*/
s32 CARDProbeEx(s32 chan, s32* memSize, s32* sectorSize) {
    if (!CARDProbe(chan)) {
        return CARD_RESULT_NOCARD;
    }
    
    if (memSize) {
        *memSize = 16;  // 16 Mbit
    }
    if (sectorSize) {
        *sectorSize = CARD_BLOCK_SIZE;
    }
    
    return 16;
}

/*---------------------------------------------------------------------------*
  Name:         CARDGetResultCode

  Description:  Get result code from last operation.

  Arguments:    chan  Card channel

  Returns:      Result code
 *---------------------------------------------------------------------------*/
s32 CARDGetResultCode(s32 chan) {
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    return __CARDCards[chan].lastResult;
}

/*---------------------------------------------------------------------------*
  Name:         CARDFreeBlocks

  Description:  Get free space on card.

  Arguments:    chan          Card channel
                bytesNotUsed  Pointer to receive free bytes
                filesNotUsed  Pointer to receive free file slots

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDFreeBlocks(s32 chan, s32* bytesNotUsed, s32* filesNotUsed) {
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!__CARDCards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    if (bytesNotUsed) {
        *bytesNotUsed = 16 * 1024 * 1024;
    }
    if (filesNotUsed) {
        *filesNotUsed = 127;
    }
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDGetMemSize

  Description:  Get memory card capacity.

  Arguments:    chan  Card channel
                size  Pointer to receive size (in Mbits)

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDGetMemSize(s32 chan, u16* size) {
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!__CARDCards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    if (size) {
        *size = 16;
    }
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDGetSectorSize

  Description:  Get sector size.

  Arguments:    chan  Card channel
                size  Pointer to receive size

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDGetSectorSize(s32 chan, u32* size) {
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!__CARDCards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    if (size) {
        *size = CARD_BLOCK_SIZE;
    }
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDGetEncoding

  Description:  Get character encoding.

  Arguments:    chan    Card channel
                encode  Pointer to receive encoding

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDGetEncoding(s32 chan, u16* encode) {
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!__CARDCards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    if (encode) {
        *encode = 1;  // ANSI
    }
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDSetDiskID

  Description:  Set disc ID.

  Arguments:    chan    Card channel
                diskID  Disc ID

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDSetDiskID(s32 chan, const DVDDiskID* diskID) {
    if (chan < 0 || chan >= CARD_MAX_CHAN || !diskID) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    memcpy(&__CARDCards[chan].diskID, diskID, sizeof(DVDDiskID));
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDSetFastMode / CARDGetFastMode

  Description:  Fast mode control.
 *---------------------------------------------------------------------------*/
BOOL CARDSetFastMode(BOOL enable) {
    (void)enable;
    return TRUE;
}

BOOL CARDGetFastMode(void) {
    return TRUE;
}

s32 CARDGetCurrentMode(s32 chan, u32* mode) {
    if (mode) *mode = 0;
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDGetXferredBytes

  Description:  Get number of bytes transferred in last operation.

  Arguments:    chan  Card channel

  Returns:      Bytes transferred
 *---------------------------------------------------------------------------*/
s32 CARDGetXferredBytes(s32 chan) {
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return 0;
    }
    
    // On PC, all transfers are instant
    return 0;
}

