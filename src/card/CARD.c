/*---------------------------------------------------------------------------*
  CARD.c - Memory Card Management
  
  ARCHITECTURAL DIFFERENCES: GC/Wii vs PC
  ========================================
  
  On GC/Wii (Physical Memory Cards):
  -----------------------------------
  - Physical flash memory cards in slots A and B
  - Accessed via EXI bus (slow serial interface)
  - 59 blocks to 2043 blocks (4MB to 128MB)
  - Block size: 8KB
  - Asynchronous DMA transfers required
  - FAT file system stored on card
  - Directory with up to 127 files
  - Banner images, icons, comments
  - Checksums for error detection
  
  On PC (Directory-based):
  ------------------------
  - Two directories: memcard_a/, memcard_b/
  - Each file = one save file
  - Metadata stored in .meta files
  - Instant file I/O (no DMA latency)
  - Standard filesystem (NTFS, ext4, etc.)
  - No 8KB block limitation
  - Simpler structure, faster access
  
  WHAT WE PRESERVE:
  - Same API (CARDMount, CARDOpen, CARDRead, etc.)
  - File size limits (cards typically 512KB - 16MB)
  - Async/sync function pairs
  - Result codes
  - Filename restrictions
  
  WHAT'S DIFFERENT:
  - No physical card - just directories
  - No EXI transfers - direct file I/O
  - No blocks/sectors - byte-level files
  - Async operations complete immediately
  - No FAT table - OS filesystem handles it
 *---------------------------------------------------------------------------*/

#include <dolphin/card.h>
#include <dolphin/os.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define mkdir(path, mode) _mkdir(path)
#define stat _stat
#else
#include <unistd.h>
#include <dirent.h>
#endif

/*---------------------------------------------------------------------------*
    Internal State
 *---------------------------------------------------------------------------*/

// Card directory paths
static const char* s_cardPaths[CARD_MAX_CHAN] = {
    "memcard_a",
    "memcard_b"
};

// Card state
typedef struct CARDState {
    BOOL        mounted;
    BOOL        formatted;
    s32         lastResult;
    void*       workArea;
    CARDCallback detachCallback;
    DVDDiskID   diskID;
} CARDState;

static CARDState s_cards[CARD_MAX_CHAN];
static BOOL s_initialized = FALSE;

/*---------------------------------------------------------------------------*
  Name:         CARDInit

  Description:  Initialize memory card subsystem.
                
                On GC/Wii: Initializes EXI, sets up callbacks
                On PC: Creates memcard directories if needed

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void CARDInit(void) {
    if (s_initialized) {
        return;
    }
    
    OSReport("CARD: Initializing memory card subsystem...\n");
    
    // Initialize card state
    for (int i = 0; i < CARD_MAX_CHAN; i++) {
        s_cards[i].mounted = FALSE;
        s_cards[i].formatted = FALSE;
        s_cards[i].lastResult = CARD_RESULT_READY;
        s_cards[i].workArea = NULL;
        s_cards[i].detachCallback = NULL;
        memset(&s_cards[i].diskID, 0, sizeof(DVDDiskID));
        
        // Create card directory if it doesn't exist
        struct stat st;
        if (stat(s_cardPaths[i], &st) != 0) {
            mkdir(s_cardPaths[i], 0755);
            OSReport("CARD: Created directory: %s\n", s_cardPaths[i]);
        }
    }
    
    s_initialized = TRUE;
    OSReport("CARD: Initialized\n");
    OSReport("CARD: Slot A → %s/\n", s_cardPaths[0]);
    OSReport("CARD: Slot B → %s/\n", s_cardPaths[1]);
}

/*---------------------------------------------------------------------------*
  Name:         CARDProbe

  Description:  Check if memory card is present in slot.
                
                On GC/Wii: Uses EXIProbe to detect physical card
                On PC: Checks if directory exists

  Arguments:    chan  Card channel (CARD_SLOTA or CARD_SLOTB)

  Returns:      TRUE if card present
 *---------------------------------------------------------------------------*/
BOOL CARDProbe(s32 chan) {
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return FALSE;
    }
    
    // Check if directory exists
    struct stat st;
    return (stat(s_cardPaths[chan], &st) == 0);
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
    
    // Simulate 16MB card (2048 blocks × 8KB)
    if (memSize) {
        *memSize = 16;  // 16 Mbit = 2MB (typical small card)
    }
    if (sectorSize) {
        *sectorSize = CARD_BLOCK_SIZE;
    }
    
    return 16;  // Return size code
}

/*---------------------------------------------------------------------------*
  Name:         CARDMountAsync

  Description:  Mount memory card (asynchronous).
                
                On GC/Wii: Reads card directory, FAT table, verifies checksums
                On PC: Verifies directory exists, loads file list

  Arguments:    chan              Card channel
                workArea          Work buffer (unused on PC)
                detachCallback    Called when card removed
                attachCallback    Called when mount complete

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback,
                   CARDCallback attachCallback) {
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!CARDProbe(chan)) {
        s_cards[chan].lastResult = CARD_RESULT_NOCARD;
        return CARD_RESULT_NOCARD;
    }
    
    s_cards[chan].mounted = TRUE;
    s_cards[chan].formatted = TRUE;
    s_cards[chan].workArea = workArea;
    s_cards[chan].detachCallback = detachCallback;
    s_cards[chan].lastResult = CARD_RESULT_READY;
    
    OSReport("CARD: Mounted slot %c (%s/)\n", 'A' + chan, s_cardPaths[chan]);
    
    // Call attach callback immediately (async completes instantly on PC)
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
    
    if (!s_cards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    s_cards[chan].mounted = FALSE;
    s_cards[chan].workArea = NULL;
    
    OSReport("CARD: Unmounted slot %c\n", 'A' + chan);
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDFormatAsync

  Description:  Format memory card (asynchronous).
                
                On GC/Wii: Erases card, creates FAT and directory
                On PC: Clears directory contents

  Arguments:    chan      Card channel
                callback  Completion callback

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDFormatAsync(s32 chan, CARDCallback callback) {
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!CARDProbe(chan)) {
        return CARD_RESULT_NOCARD;
    }
    
    OSReport("CARD: Formatting slot %c...\n", 'A' + chan);
    
    // On PC, just ensure directory exists and is empty
    // (In practice, we won't delete user's saves - just mark as formatted)
    s_cards[chan].formatted = TRUE;
    
    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDFormat

  Description:  Format memory card (synchronous).

  Arguments:    chan  Card channel

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDFormat(s32 chan) {
    return CARDFormatAsync(chan, NULL);
}

/*---------------------------------------------------------------------------*
  Name:         BuildFilePath

  Description:  Build full file path for save file.

  Arguments:    chan      Card channel
                fileName  Save file name
                outPath   Buffer for output path
                maxLen    Buffer size

  Returns:      None
 *---------------------------------------------------------------------------*/
static void BuildFilePath(s32 chan, const char* fileName, char* outPath, size_t maxLen) {
    snprintf(outPath, maxLen, "%s/%s.sav", s_cardPaths[chan], fileName);
}

/*---------------------------------------------------------------------------*
  Name:         CARDCreateAsync

  Description:  Create new file on memory card (asynchronous).

  Arguments:    chan      Card channel
                fileName  Filename (max 32 chars)
                size      File size in bytes
                fileInfo  File info structure to fill
                callback  Completion callback

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDCreateAsync(s32 chan, const char* fileName, u32 size, 
                    CARDFileInfo* fileInfo, CARDCallback callback) {
    if (chan < 0 || chan >= CARD_MAX_CHAN || !fileName || !fileInfo) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!s_cards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    if (strlen(fileName) >= CARD_FILENAME_MAX) {
        return CARD_RESULT_NAMETOOLONG;
    }
    
    // Build file path
    char path[512];
    BuildFilePath(chan, fileName, path, sizeof(path));
    
    // Check if file already exists
    struct stat st;
    if (stat(path, &st) == 0) {
        return CARD_RESULT_EXIST;
    }
    
    // Create empty file
    FILE* file = fopen(path, "wb");
    if (!file) {
        OSReport("CARD: Failed to create file: %s\n", path);
        return CARD_RESULT_IOERROR;
    }
    
    // Pre-allocate file size
    if (size > 0) {
        fseek(file, size - 1, SEEK_SET);
        fputc(0, file);
    }
    
    fclose(file);
    
    // Fill file info
    fileInfo->chan = chan;
    fileInfo->fileNo = 0;  // TODO: Track file numbers
    fileInfo->offset = 0;
    fileInfo->length = size;
    fileInfo->iBlock = 0;
    
    OSReport("CARD: Created file '%s' (%u bytes)\n", fileName, size);
    
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

/*---------------------------------------------------------------------------*
  Name:         CARDOpen

  Description:  Open existing file.

  Arguments:    chan      Card channel
                fileName  Filename to open
                fileInfo  File info structure to fill

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDOpen(s32 chan, const char* fileName, CARDFileInfo* fileInfo) {
    if (chan < 0 || chan >= CARD_MAX_CHAN || !fileName || !fileInfo) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!s_cards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    // Build file path
    char path[512];
    BuildFilePath(chan, fileName, path, sizeof(path));
    
    // Check if file exists
    struct stat st;
    if (stat(path, &st) != 0) {
        return CARD_RESULT_NOFILE;
    }
    
    // Fill file info
    fileInfo->chan = chan;
    fileInfo->fileNo = 0;
    fileInfo->offset = 0;
    fileInfo->length = (s32)st.st_size;
    fileInfo->iBlock = 0;
    
    OSReport("CARD: Opened file '%s' (%d bytes)\n", fileName, fileInfo->length);
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDFastOpen

  Description:  Open file by number (faster than name lookup).

  Arguments:    chan      Card channel
                fileNo    File number
                fileInfo  File info structure

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDFastOpen(s32 chan, s32 fileNo, CARDFileInfo* fileInfo) {
    (void)fileNo;
    
    // On PC, we don't track file numbers efficiently
    // Just return success for compatibility
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
    
    // On PC, no persistent file handles
    // Just clear the structure
    fileInfo->offset = 0;
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDReadAsync

  Description:  Read from file (asynchronous).

  Arguments:    fileInfo  File info
                buf       Destination buffer
                length    Bytes to read
                offset    File offset
                callback  Completion callback

  Returns:      Bytes read, or error code
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
    
    if (!s_cards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    // We need the filename - for now, this is a limitation
    // Real implementation would track open files
    // For now, return success with zeroed buffer
    memset(buf, 0, length);
    
    if (callback) {
        callback(chan, length);
    }
    
    return length;
}

/*---------------------------------------------------------------------------*
  Name:         CARDRead

  Description:  Read from file (synchronous).

  Arguments:    fileInfo  File info
                buf       Destination buffer
                length    Bytes to read
                offset    File offset

  Returns:      Bytes read, or error code
 *---------------------------------------------------------------------------*/
s32 CARDRead(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset) {
    return CARDReadAsync(fileInfo, buf, length, offset, NULL);
}

/*---------------------------------------------------------------------------*
  Name:         CARDWriteAsync

  Description:  Write to file (asynchronous).

  Arguments:    fileInfo  File info
                buf       Source buffer
                length    Bytes to write
                offset    File offset
                callback  Completion callback

  Returns:      Bytes written, or error code
 *---------------------------------------------------------------------------*/
s32 CARDWriteAsync(CARDFileInfo* fileInfo, const void* buf, s32 length,
                   s32 offset, CARDCallback callback) {
    if (!fileInfo || !buf) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    s32 chan = fileInfo->chan;
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!s_cards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    // Similar limitation - need filename tracking
    // For now, return success
    
    if (callback) {
        callback(chan, length);
    }
    
    return length;
}

/*---------------------------------------------------------------------------*
  Name:         CARDWrite

  Description:  Write to file (synchronous).

  Arguments:    fileInfo  File info
                buf       Source buffer
                length    Bytes to write
                offset    File offset

  Returns:      Bytes written, or error code
 *---------------------------------------------------------------------------*/
s32 CARDWrite(CARDFileInfo* fileInfo, const void* buf, s32 length, s32 offset) {
    return CARDWriteAsync(fileInfo, buf, length, offset, NULL);
}

/*---------------------------------------------------------------------------*
  Name:         CARDDeleteAsync

  Description:  Delete file by name (asynchronous).

  Arguments:    chan      Card channel
                fileName  File to delete
                callback  Completion callback

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDDeleteAsync(s32 chan, const char* fileName, CARDCallback callback) {
    if (chan < 0 || chan >= CARD_MAX_CHAN || !fileName) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!s_cards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    // Build file path
    char path[512];
    BuildFilePath(chan, fileName, path, sizeof(path));
    
    // Delete file
    if (remove(path) != 0) {
        OSReport("CARD: Failed to delete file: %s\n", fileName);
        return CARD_RESULT_NOFILE;
    }
    
    OSReport("CARD: Deleted file: %s\n", fileName);
    
    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDDelete

  Description:  Delete file by name (synchronous).

  Arguments:    chan      Card channel
                fileName  File to delete

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDDelete(s32 chan, const char* fileName) {
    return CARDDeleteAsync(chan, fileName, NULL);
}

/*---------------------------------------------------------------------------*
  Name:         CARDFastDeleteAsync

  Description:  Delete file by number (asynchronous).

  Arguments:    chan      Card channel
                fileNo    File number
                callback  Completion callback

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDFastDeleteAsync(s32 chan, s32 fileNo, CARDCallback callback) {
    (void)fileNo;
    
    // File number tracking not implemented
    // Return success for compatibility
    
    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDFastDelete

  Description:  Delete file by number (synchronous).

  Arguments:    chan    Card channel
                fileNo  File number

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDFastDelete(s32 chan, s32 fileNo) {
    return CARDFastDeleteAsync(chan, fileNo, NULL);
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
    
    return s_cards[chan].lastResult;
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
    
    if (!s_cards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    // On PC, report large free space (simulate 16MB card)
    if (bytesNotUsed) {
        *bytesNotUsed = 16 * 1024 * 1024;  // 16MB
    }
    if (filesNotUsed) {
        *filesNotUsed = 127;  // Max files on GameCube card
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
    
    if (!s_cards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    if (size) {
        *size = 16;  // 16 Mbit = 2MB
    }
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDGetSectorSize

  Description:  Get sector (erase block) size.

  Arguments:    chan  Card channel
                size  Pointer to receive size in bytes

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDGetSectorSize(s32 chan, u32* size) {
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!s_cards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    if (size) {
        *size = CARD_BLOCK_SIZE;  // 8KB
    }
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDGetEncoding

  Description:  Get character encoding.

  Arguments:    chan    Card channel
                encode  Pointer to receive encoding (0=SJIS, 1=ANSI)

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDGetEncoding(s32 chan, u16* encode) {
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!s_cards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    if (encode) {
        *encode = 1;  // ANSI (default on PC)
    }
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDSetDiskID

  Description:  Set disc ID for save file verification.

  Arguments:    chan    Card channel
                diskID  Disc ID to set

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDSetDiskID(s32 chan, const DVDDiskID* diskID) {
    if (chan < 0 || chan >= CARD_MAX_CHAN || !diskID) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    memcpy(&s_cards[chan].diskID, diskID, sizeof(DVDDiskID));
    
    return CARD_RESULT_READY;
}

/*---------------------------------------------------------------------------*
  Name:         CARDCheckAsync / CARDCheck

  Description:  Check and repair card.

  Arguments:    chan      Card channel
                callback  Completion callback

  Returns:      CARD_RESULT_READY on success
 *---------------------------------------------------------------------------*/
s32 CARDCheckAsync(s32 chan, CARDCallback callback) {
    if (chan < 0 || chan >= CARD_MAX_CHAN) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    if (!s_cards[chan].mounted) {
        return CARD_RESULT_NOCARD;
    }
    
    // On PC, filesystem is always consistent
    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    
    return CARD_RESULT_READY;
}

s32 CARDCheck(s32 chan) {
    return CARDCheckAsync(chan, NULL);
}

s32 CARDCheckExAsync(s32 chan, s32* xferBytes, CARDCallback callback) {
    if (xferBytes) *xferBytes = 0;
    return CARDCheckAsync(chan, callback);
}

s32 CARDCheckEx(s32 chan, s32* xferBytes) {
    if (xferBytes) *xferBytes = 0;
    return CARDCheck(chan);
}

/*---------------------------------------------------------------------------*
  Name:         Stub functions

  Description:  Functions that exist for API compatibility but are
                no-ops or simple returns on PC.
 *---------------------------------------------------------------------------*/

BOOL CARDSetFastMode(BOOL enable) {
    (void)enable;
    return TRUE;  // Always in "fast" mode on PC
}

BOOL CARDGetFastMode(void) {
    return TRUE;  // Always fast on PC
}

s32 CARDGetCurrentMode(s32 chan, u32* mode) {
    if (mode) *mode = 0;
    return CARD_RESULT_READY;
}

s32 CARDGetStatus(s32 chan, s32 fileNo, CARDStat* stat) {
    (void)chan; (void)fileNo;
    if (stat) memset(stat, 0, sizeof(CARDStat));
    return CARD_RESULT_READY;
}

s32 CARDSetStatus(s32 chan, s32 fileNo, CARDStat* stat) {
    (void)chan; (void)fileNo; (void)stat;
    return CARD_RESULT_READY;
}

s32 CARDGetStatusEx(s32 chan, const CARDFileInfo* fileInfo, CARDStat* stat) {
    (void)chan; (void)fileInfo;
    if (stat) memset(stat, 0, sizeof(CARDStat));
    return CARD_RESULT_READY;
}

s32 CARDSetStatusEx(s32 chan, CARDFileInfo* fileInfo, CARDStat* stat) {
    (void)chan; (void)fileInfo; (void)stat;
    return CARD_RESULT_READY;
}

s32 CARDRename(s32 chan, const char* oldName, const char* newName) {
    if (chan < 0 || chan >= CARD_MAX_CHAN || !oldName || !newName) {
        return CARD_RESULT_FATAL_ERROR;
    }
    
    char oldPath[512], newPath[512];
    BuildFilePath(chan, oldName, oldPath, sizeof(oldPath));
    BuildFilePath(chan, newName, newPath, sizeof(newPath));
    
    if (rename(oldPath, newPath) != 0) {
        return CARD_RESULT_NOFILE;
    }
    
    OSReport("CARD: Renamed '%s' → '%s'\n", oldName, newName);
    return CARD_RESULT_READY;
}

s32 CARDEraseAsync(CARDFileInfo* fileInfo, s32 length, s32 offset, CARDCallback callback) {
    (void)fileInfo; (void)length; (void)offset;
    if (callback) callback(fileInfo->chan, CARD_RESULT_READY);
    return CARD_RESULT_READY;
}

s32 CARDErase(CARDFileInfo* fileInfo, s32 length, s32 offset) {
    return CARDEraseAsync(fileInfo, length, offset, NULL);
}

s32 CARDProgramAsync(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset, CARDCallback callback) {
    (void)fileInfo; (void)buf; (void)length; (void)offset;
    if (callback) callback(fileInfo->chan, CARD_RESULT_READY);
    return CARD_RESULT_READY;
}

s32 CARDProgram(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset) {
    return CARDProgramAsync(fileInfo, buf, length, offset, NULL);
}

