/*---------------------------------------------------------------------------*
  CARDRename.c - File Rename Operations
 *---------------------------------------------------------------------------*/

#include <dolphin/card.h>
#include <dolphin/card_internal.h>
#include <dolphin/os.h>
#include <stdio.h>

/*---------------------------------------------------------------------------*
  Name:         CARDRename

  Description:  Rename file.

  Arguments:    chan     Card channel
                oldName  Current filename
                newName  New filename

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDRename(s32 chan, const char* oldName, const char* newName) {
    if (chan < 0 || chan >= CARD_MAX_CHAN || !oldName || !newName) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    char oldPath[512], newPath[512];
    __CARDBuildFilePath(chan, oldName, oldPath, sizeof(oldPath));
    __CARDBuildFilePath(chan, newName, newPath, sizeof(newPath));
    
    if (rename(oldPath, newPath) != 0) {
        return CARD_RESULT_NOFILE;
    }
    
    OSReport("CARD: Renamed '%s' → '%s'\n", oldName, newName);
    return CARD_RESULT_READY;
}

