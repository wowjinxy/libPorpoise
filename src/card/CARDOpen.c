/*---------------------------------------------------------------------------*
  CARDOpen.c - File Opening Operations
 *---------------------------------------------------------------------------*/

#include <dolphin/card.h>
#include <dolphin/card_internal.h>
#include <dolphin/os.h>
#include <sys/stat.h>
#include <string.h>

#ifdef _WIN32
#define stat _stat
#endif

/*---------------------------------------------------------------------------*
  Name:         CARDOpen

  Description:  Open existing file.

  Arguments:    chan      Card channel
                fileName  Filename
                fileInfo  File info structure

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDOpen(s32 chan, const char* fileName, CARDFileInfo* fileInfo) {
    if (chan < 0 || chan >= CARD_MAX_CHAN || !fileName || !fileInfo) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!__CARDCards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    char path[512];
    __CARDBuildFilePath(chan, fileName, path, sizeof(path));
    
    struct stat st;
    if (stat(path, &st) != 0) {
        return CARD_RESULT_NOFILE;
    }
    
    fileInfo->chan = chan;
    fileInfo->fileNo = 0;
    fileInfo->offset = 0;
    fileInfo->length = (s32)st.st_size;
    fileInfo->iBlock = 0;
    
    OSReport("CARD: Opened '%s' (%d bytes)\n", fileName, fileInfo->length);
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDFastOpen

  Description:  Open file by number.

  Arguments:    chan      Card channel
                fileNo    File number
                fileInfo  File info structure

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDFastOpen(s32 chan, s32 fileNo, CARDFileInfo* fileInfo) {
    (void)fileNo;
    
    if (!fileInfo) return CARD_RESULT_FATAL_ERROR;
    
    fileInfo->chan = chan;
    fileInfo->fileNo = fileNo;
    fileInfo->offset = 0;
    fileInfo->length = 0;
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDClose

  Description:  Close file.

  Arguments:    fileInfo  File info structure

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDClose(CARDFileInfo* fileInfo) {
    if (!fileInfo) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    fileInfo->offset = 0;
    
    return CARD_RESULT_READY;
}

