/*---------------------------------------------------------------------------*
  CARDCreate.c - File Creation Operations
 *---------------------------------------------------------------------------*/

#include <dolphin/card.h>
#include <dolphin/card_internal.h>
#include <dolphin/os.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#define stat _stat
#endif

/*---------------------------------------------------------------------------*
  Name:         CARDCreateAsync

  Description:  Create new file (asynchronous).

  Arguments:    chan      Card channel
                fileName  Filename
                size      File size
                fileInfo  File info structure
                callback  Completion callback

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDCreateAsync(s32 chan, const char* fileName, u32 size,
                    CARDFileInfo* fileInfo, CARDCallback callback) {
    if (chan < 0 || chan >= CARD_MAX_CHAN || !fileName || !fileInfo) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!__CARDCards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    if (strlen(fileName) >= CARD_FILENAME_MAX) {
        return CARD_RESULT_NAMETOOLONG;
    }
    
    char path[512];
    __CARDBuildFilePath(chan, fileName, path, sizeof(path));
    
    struct stat st;
    if (stat(path, &st) == 0) {
        return CARD_RESULT_EXIST;
    }
    
    FILE* file = fopen(path, "wb");
    if (!file) {
        OSReport("CARD: Failed to create '%s'\n", path);
        return CARD_RESULT_IOERROR;
    }
    
    if (size > 0) {
        fseek(file, size - 1, SEEK_SET);
        fputc(0, file);
    }
    
    fclose(file);
    
    fileInfo->chan = chan;
    fileInfo->fileNo = 0;
    fileInfo->offset = 0;
    fileInfo->length = size;
    fileInfo->iBlock = 0;
    
    OSReport("CARD: Created '%s' (%u bytes)\n", fileName, size);
    
    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDCreate

  Description:  Create new file (synchronous).

  Arguments:    chan      Card channel
                fileName  Filename
                size      File size
                fileInfo  File info structure

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDCreate(s32 chan, const char* fileName, u32 size, CARDFileInfo* fileInfo) {
    return CARDCreateAsync(chan, fileName, size, fileInfo, NULL);
}

