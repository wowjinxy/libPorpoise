/*---------------------------------------------------------------------------*
  CARDCreate.c - File Creation Operations
 *---------------------------------------------------------------------------*/

#include <dolphin/card.h>
#include <dolphin/card_internal.h>
#include <dolphin/os.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>

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
    
    // Find free file slot and store filename
    s32 fileNo = -1;
    for (int i = 0; i < 127; i++) {
        if (__CARDCards[chan].openFiles[i][0] == '\0') {
            fileNo = i;
            break;
        }
    }
    
    if (fileNo < 0) {
        return CARD_RESULT_LIMIT;
    }
    
    // Store filename for read/write operations
    strncpy(__CARDCards[chan].openFiles[fileNo], fileName, CARD_FILENAME_MAX - 1);
    __CARDCards[chan].openFiles[fileNo][CARD_FILENAME_MAX - 1] = '\0';
    
    fileInfo->chan = chan;
    fileInfo->fileNo = fileNo;
    fileInfo->offset = 0;
    fileInfo->length = size;
    fileInfo->iBlock = 0;
    
    OSReport("CARD: Created '%s' (%u bytes) [fileNo=%d]\n", fileName, size, fileNo);
    
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

