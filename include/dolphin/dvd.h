/**
 * @file dvd.h
 * @brief DVD Disc I/O API for libPorpoise
 * 
 * Provides disc file system access compatible with GameCube/Wii SDK.
 * On PC, maps to local filesystem directory (default: "./files/")
 */

#ifndef DOLPHIN_DVD_H
#define DOLPHIN_DVD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dolphin/types.h>

// Forward declarations
typedef struct OSAlarm OSAlarm;

/*---------------------------------------------------------------------------*
    Constants
 *---------------------------------------------------------------------------*/

// DVD result codes
#define DVD_RESULT_GOOD         0
#define DVD_RESULT_FATAL_ERROR  -1
#define DVD_RESULT_IGNORED      -2
#define DVD_RESULT_CANCELED     -3

// DVD state codes
#define DVD_STATE_END           0
#define DVD_STATE_BUSY          1
#define DVD_STATE_WAITING       2
#define DVD_STATE_COVER_CLOSED  3
#define DVD_STATE_NO_DISK       4
#define DVD_STATE_COVER_OPEN    5
#define DVD_STATE_WRONG_DISK    6
#define DVD_STATE_MOTOR_STOPPED 7
#define DVD_STATE_PAUSING       8
#define DVD_STATE_IGNORED       9
#define DVD_STATE_CANCELED      10
#define DVD_STATE_RETRY         11

// DVD priority levels
#define DVD_PRIO_HIGH           4
#define DVD_PRIO_MEDIUM         2
#define DVD_PRIO_LOW            0

/*---------------------------------------------------------------------------*
    Types
 *---------------------------------------------------------------------------*/

/**
 * @brief DVD command block (internal state)
 */
typedef struct DVDCommandBlock DVDCommandBlock;

/**
 * @brief DVD file information structure
 */
typedef struct DVDFileInfo {
    DVDCommandBlock* cb;            ///< Command block pointer
    u32              startAddr;     ///< File start address (offset in bytes)
    u32              length;        ///< File length in bytes
    void*            callback;      ///< Callback function
} DVDFileInfo;

/**
 * @brief DVD directory entry structure
 */
typedef struct DVDDirEntry {
    u32   entryNum;                 ///< Entry number in FST
    BOOL  isDir;                    ///< TRUE if directory, FALSE if file
    char* name;                     ///< Entry name (points to FST string table)
} DVDDirEntry;

/**
 * @brief DVD directory structure
 */
typedef struct DVDDir {
    u32  entryNum;                  ///< Current entry number
    u32  location;                  ///< Current location in directory
    u32  next;                      ///< Next entry number
} DVDDir;

/**
 * @brief DVD disk ID structure
 */
typedef struct DVDDiskID {
    char    gameName[4];            ///< Game code (e.g., "GMSE")
    char    company[2];             ///< Company code (e.g., "01")
    u8      diskNumber;             ///< Disc number
    u8      gameVersion;            ///< Game version
    u8      streaming;              ///< Audio streaming flag
    u8      streamBufSize;          ///< Streaming buffer size
    u8      padding[14];            ///< Padding
    u32     rvlMagic;              ///< Wii magic word
    u32     gcMagic;               ///< GC magic word
} DVDDiskID;

/**
 * @brief DVD callback function type
 */
typedef void (*DVDCallback)(s32 result, DVDFileInfo* fileInfo);

/**
 * @brief DVD command block callback type
 */
typedef void (*DVDCBCallback)(s32 result, DVDCommandBlock* block);

/*---------------------------------------------------------------------------*
    Functions
 *---------------------------------------------------------------------------*/

/**
 * @brief Initialize the DVD subsystem
 * 
 * Mounts the "files/" directory as the DVD root. All disc paths
 * are resolved relative to this directory.
 * 
 * @return TRUE if initialization succeeded
 */
BOOL DVDInit(void);

/**
 * @brief Open a file from disc
 * 
 * Opens a file for reading. Path is relative to DVD root ("files/").
 * Example: DVDOpen("data/stage1.dat", &file) opens "files/data/stage1.dat"
 * 
 * @param fileName Path to file (relative to DVD root)
 * @param fileInfo Pointer to DVDFileInfo structure to fill
 * @return TRUE if file opened successfully, FALSE if not found
 */
BOOL DVDOpen(const char* fileName, DVDFileInfo* fileInfo);

/**
 * @brief Close a file
 * 
 * @param fileInfo Pointer to open file
 * @return TRUE if closed successfully
 */
BOOL DVDClose(DVDFileInfo* fileInfo);

/**
 * @brief Read from file (synchronous)
 * 
 * Reads data from file at current position. Blocks until complete.
 * 
 * @param fileInfo  Pointer to open file
 * @param addr      Destination buffer
 * @param length    Number of bytes to read
 * @param offset    Offset in file to start reading from
 * @return Number of bytes read, or -1 on error
 */
s32 DVDRead(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset);

/**
 * @brief Read from file with priority (synchronous)
 * 
 * @param fileInfo  Pointer to open file
 * @param addr      Destination buffer
 * @param length    Number of bytes to read
 * @param offset    Offset in file to start reading from
 * @param prio      Priority (DVD_PRIO_*)
 * @return Number of bytes read, or -1 on error
 */
s32 DVDReadPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, s32 prio);

/**
 * @brief Read from file (asynchronous)
 * 
 * Starts a read operation that completes in the background.
 * Callback is called when complete.
 * 
 * @param fileInfo  Pointer to open file
 * @param addr      Destination buffer
 * @param length    Number of bytes to read
 * @param offset    Offset in file to start reading from
 * @param callback  Callback function (or NULL)
 * @return TRUE if read started successfully
 */
BOOL DVDReadAsync(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset,
                  DVDCallback callback);

/**
 * @brief Read from file (asynchronous with priority)
 * 
 * @param fileInfo  Pointer to open file
 * @param addr      Destination buffer
 * @param length    Number of bytes to read
 * @param offset    Offset in file to start reading from
 * @param callback  Callback function (or NULL)
 * @param prio      Priority (DVD_PRIO_*)
 * @return TRUE if read started successfully
 */
BOOL DVDReadAsyncPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset,
                      DVDCallback callback, s32 prio);

/**
 * @brief Seek to position in file (synchronous)
 * 
 * @param fileInfo  Pointer to open file
 * @param offset    Offset to seek to
 * @return Current position in file
 */
s32 DVDSeek(DVDFileInfo* fileInfo, s32 offset);

/**
 * @brief Seek with priority (synchronous)
 * 
 * @param fileInfo  Pointer to open file
 * @param offset    Offset to seek to
 * @param prio      Priority (DVD_PRIO_*)
 * @return Current position in file
 */
s32 DVDSeekPrio(DVDFileInfo* fileInfo, s32 offset, s32 prio);

/**
 * @brief Seek asynchronously
 * 
 * @param fileInfo  Pointer to open file
 * @param offset    Offset to seek to
 * @param callback  Callback when complete
 * @return TRUE if seek started
 */
BOOL DVDSeekAsync(DVDFileInfo* fileInfo, s32 offset, DVDCallback callback);

/**
 * @brief Seek asynchronously with priority
 * 
 * @param fileInfo  Pointer to open file
 * @param offset    Offset to seek to
 * @param callback  Callback when complete
 * @param prio      Priority (DVD_PRIO_*)
 * @return TRUE if seek started
 */
BOOL DVDSeekAsyncPrio(DVDFileInfo* fileInfo, s32 offset, DVDCallback callback, s32 prio);

/**
 * @brief Get file information status
 * 
 * @param fileInfo  Pointer to file
 * @return DVD_STATE_* constant
 */
s32 DVDGetFileInfoStatus(const DVDFileInfo* fileInfo);

/**
 * @brief Get command block status
 * 
 * @param block  Pointer to command block
 * @return DVD_STATE_* constant
 */
s32 DVDGetCommandBlockStatus(const DVDCommandBlock* block);

/**
 * @brief Cancel a pending operation
 * 
 * @param fileInfo  Pointer to file with operation
 * @return TRUE if canceled successfully
 */
BOOL DVDCancel(DVDFileInfo* fileInfo);

/**
 * @brief Cancel operation asynchronously
 * 
 * @param fileInfo  Pointer to file with operation
 * @param callback  Callback when cancel completes
 * @return TRUE if cancel initiated
 */
BOOL DVDCancelAsync(DVDFileInfo* fileInfo, DVDCallback callback);

/**
 * @brief Get bytes transferred for async operation
 * 
 * @param fileInfo  Pointer to file
 * @return Number of bytes transferred so far
 */
s32 DVDGetTransferredSize(DVDFileInfo* fileInfo);

/**
 * @brief Convert file path to entry number
 * 
 * @param pathPtr  Path to file or directory
 * @return Entry number, or -1 if not found
 */
s32 DVDConvertPathToEntrynum(const char* pathPtr);

/**
 * @brief Open file by entry number (fast open)
 * 
 * @param entrynum  Entry number
 * @param fileInfo  Pointer to DVDFileInfo to fill
 * @return TRUE if opened successfully
 */
BOOL DVDFastOpen(s32 entrynum, DVDFileInfo* fileInfo);

/**
 * @brief Read disc ID information
 * 
 * @param block     Command block
 * @param diskID    Pointer to DVDDiskID to fill
 * @param callback  Callback when complete
 * @return TRUE if read started
 */
BOOL DVDReadDiskID(DVDCommandBlock* block, DVDDiskID* diskID, DVDCallback callback);

/**
 * @brief Get drive status
 * 
 * @return DVD_STATE_* constant
 */
s32 DVDGetDriveStatus(void);

/**
 * @brief Check if disc is present and readable
 * 
 * @return DVD_RESULT_GOOD if OK, negative on error
 */
s32 DVDCheckDisk(void);

/**
 * @brief Resume DVD operations
 * 
 * @return TRUE if resumed
 */
BOOL DVDResume(void);

/**
 * @brief Query drive information
 * 
 * @param block     Command block
 * @param info      Pointer to drive info structure
 * @param callback  Callback when complete
 * @return DVD_RESULT_GOOD
 */
s32 DVDInquiry(DVDCommandBlock* block, void* info, DVDCBCallback callback);

/**
 * @brief Query drive information (async)
 */
BOOL DVDInquiryAsync(DVDCommandBlock* block, void* info, DVDCBCallback callback);

/**
 * @brief Change disc (sync)
 */
s32 DVDChangeDisk(DVDCommandBlock* block, DVDDiskID* diskID);

/**
 * @brief Change disc (async)
 */
BOOL DVDChangeDiskAsync(DVDCommandBlock* block, DVDDiskID* diskID, DVDCBCallback callback);

/**
 * @brief Stop disc motor (sync)
 */
s32 DVDStopMotor(DVDCommandBlock* block);

/**
 * @brief Stop disc motor (async)
 */
BOOL DVDStopMotorAsync(DVDCommandBlock* block, DVDCBCallback callback);

/**
 * @brief Reset DVD system (async)
 */
BOOL DVDResetAsync(DVDCommandBlock* block, DVDCBCallback callback);

/**
 * @brief Check if reset is required
 */
BOOL DVDResetRequired(void);

/**
 * @brief Set auto cache invalidation
 */
BOOL DVDSetAutoInvalidation(BOOL autoInval);

/**
 * @brief Cancel all operations (sync)
 */
s32 DVDCancelAll(void);

/**
 * @brief Cancel all operations (async)
 */
BOOL DVDCancelAllAsync(DVDCBCallback callback);

/**
 * @brief Prepare audio streaming (async)
 */
BOOL DVDPrepareStreamAbsAsync(DVDCommandBlock* block, u32 length, u32 offset,
                               DVDCBCallback callback);

/**
 * @brief Cancel audio streaming (async)
 */
BOOL DVDCancelStreamAsync(DVDCommandBlock* block, DVDCBCallback callback);

/**
 * @brief Cancel audio streaming (sync)
 */
s32 DVDCancelStream(DVDCommandBlock* block);

/**
 * @brief Read from absolute disc offset
 */
BOOL DVDReadAbsAsyncPrio(DVDCommandBlock* block, void* addr, s32 length,
                         u32 offset, DVDCBCallback callback, s32 prio);
BOOL DVDReadAbsAsyncForBS(DVDCommandBlock* block, void* addr, s32 length,
                          u32 offset, DVDCBCallback callback);

/*---------------------------------------------------------------------------*
    Queue Management (Internal)
 *---------------------------------------------------------------------------*/

void __DVDClearWaitingQueue(void);
BOOL __DVDPushWaitingQueue(s32 prio, DVDCommandBlock* block);
DVDCommandBlock* __DVDPopWaitingQueue(void);
BOOL __DVDCheckWaitingList(void);
BOOL __DVDDequeueWaitingQueue(DVDCommandBlock* block);
BOOL __DVDIsBlockInWaitingQueue(DVDCommandBlock* block);

/*---------------------------------------------------------------------------*
    Low-Level Commands (Internal)
 *---------------------------------------------------------------------------*/

void DVDLowInit(void);
BOOL DVDLowRead(void* addr, u32 length, u32 offset, void (*callback)(u32));
BOOL DVDLowSeek(u32 offset, void (*callback)(u32));
BOOL DVDLowWaitCoverClose(void (*callback)(u32));
BOOL DVDLowStopMotor(void (*callback)(u32));
BOOL DVDLowReadDiskID(DVDDiskID* diskID, void (*callback)(u32));
BOOL DVDLowRequestError(void (*callback)(u32));
BOOL DVDLowReset(void (*callback)(u32));
BOOL DVDLowBreak(void);
void DVDLowClearCallback(void);
BOOL __DVDLowTestAlarm(const OSAlarm* alarm);

/*---------------------------------------------------------------------------*
    Error Handling (Internal)
 *---------------------------------------------------------------------------*/

void __DVDStoreErrorCode(u32 errorCode, u32 result);
u32  __DVDGetLastError(void);
void __DVDClearErrorLog(void);
BOOL __DVDHasErrorLogged(void);

/*---------------------------------------------------------------------------*
    Fatal Error (Internal)
 *---------------------------------------------------------------------------*/

void __DVDShowFatalMessage(void);
void __DVDPrintFatalMessage(void);

/**
 * @brief Open a directory
 * 
 * @param dirName  Path to directory (relative to DVD root)
 * @param dir      Pointer to DVDDir structure to fill
 * @return TRUE if directory opened successfully
 */
BOOL DVDOpenDir(const char* dirName, DVDDir* dir);

/**
 * @brief Read next entry from directory
 * 
 * @param dir      Pointer to open directory
 * @param dirent   Pointer to DVDDirEntry to fill
 * @return TRUE if entry read, FALSE if end of directory
 */
BOOL DVDReadDir(DVDDir* dir, DVDDirEntry* dirent);

/**
 * @brief Close a directory
 * 
 * @param dir  Pointer to open directory
 * @return TRUE if closed successfully
 */
BOOL DVDCloseDir(DVDDir* dir);

/**
 * @brief Rewind directory to beginning
 * 
 * @param dir  Pointer to open directory
 */
void DVDRewindDir(DVDDir* dir);

/**
 * @brief Get current directory path
 * 
 * @param path    Buffer to receive path
 * @param maxlen  Maximum length of buffer
 * @return TRUE if successful
 */
BOOL DVDGetCurrentDir(char* path, u32 maxlen);

/**
 * @brief Change current directory
 * 
 * @param dirName  Path to new directory
 * @return TRUE if successful
 */
BOOL DVDChangeDir(const char* dirName);

/**
 * @brief Get disc ID information
 * 
 * @return Pointer to DVDDiskID structure (static data)
 */
DVDDiskID* DVDGetDiskID(void);

/**
 * @brief Set DVD root directory path
 * 
 * PC-specific extension: Changes the root directory for DVD file access.
 * Default is "./files/"
 * 
 * @param path  Path to new DVD root directory
 * @return TRUE if path is valid
 */
BOOL DVDSetRootDirectory(const char* path);

/**
 * @brief Get DVD root directory path
 * 
 * @return Current DVD root directory path
 */
const char* DVDGetRootDirectory(void);

#ifdef __cplusplus
}
#endif

#endif // DOLPHIN_DVD_H

