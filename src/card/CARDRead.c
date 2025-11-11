/*---------------------------------------------------------------------------*
  CARDRead.c - File Read Operations
 *---------------------------------------------------------------------------*/

#include <dolphin/card.h>
#include <dolphin/card_internal.h>
#include <dolphin/os.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*
  Name:         CARDReadAsync

  Description:  Read from file (asynchronous).

  Arguments:    fileInfo  File info
                buf       Destination buffer
                length    Bytes to read
                offset    File offset
                callback  Completion callback

  Returns:      Bytes read, or error
 *---------------------------------------------------------------------------*/
s32 CARDReadAsync(CARDFileInfo* fileInfo, void* buf, s32 length,
                  s32 offset, CARDCallback callback) {
    if (!fileInfo || !buf) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    s32 chan = fileInfo->chan;
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!__CARDCards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    // Get filename from file number
    s32 fileNo = fileInfo->fileNo;
    if (fileNo < 0 || fileNo >= 127 || __CARDCards[chan].openFiles[fileNo][0] == '\0') {
        return CARD_RESULT_FATAL_ERROR;  // File not open
    }
    
    const char* fileName = __CARDCards[chan].openFiles[fileNo];
    char path[512];
    __CARDBuildFilePath(chan, fileName, path, sizeof(path));
    
    // Read from actual file
    FILE* file = fopen(path, "rb");
    if (!file) {
        return CARD_RESULT_IOERROR;
    }
    
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return CARD_RESULT_IOERROR;
    }
    
    size_t bytesRead = fread(buf, 1, length, file);
    fclose(file);
    
    if (callback) {
        callback(chan, (s32)bytesRead);
    }
    
    return (s32)bytesRead;
}

/*---------------------------------------------------------------------------*
  Name:         CARDRead

  Description:  Read from file (synchronous).

  Arguments:    fileInfo  File info
                buf       Destination buffer
                length    Bytes to read
                offset    File offset

  Returns:      Bytes read, or error
 *---------------------------------------------------------------------------*/
s32 CARDRead(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset) {
    return CARDReadAsync(fileInfo, buf, length, offset, NULL);
}

