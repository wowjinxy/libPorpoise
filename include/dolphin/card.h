/**
 * @file card.h
 * @brief CARD (Memory Card) API for libPorpoise
 * 
 * On GameCube/Wii: Manages save data on physical memory cards via EXI
 * On PC: Maps to directories (memcard_a/, memcard_b/) with individual save files
 */

#ifndef DOLPHIN_CARD_H
#define DOLPHIN_CARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dolphin/types.h>
#include <dolphin/dvd.h>  // For DVDDiskID

/*---------------------------------------------------------------------------*
    Constants
 *---------------------------------------------------------------------------*/

// Memory card channels (Slot A and Slot B)
#define CARD_SLOTA      0
#define CARD_SLOTB      1
#define CARD_MAX_CHAN   2

// Result codes
#define CARD_RESULT_READY           1
#define CARD_RESULT_BUSY            0
#define CARD_RESULT_WRONGDEVICE    -1
#define CARD_RESULT_NOCARD         -2
#define CARD_RESULT_NOFILE         -3
#define CARD_RESULT_IOERROR        -4
#define CARD_RESULT_BROKEN         -5
#define CARD_RESULT_EXIST          -6
#define CARD_RESULT_NOENT          -7
#define CARD_RESULT_INSSPACE       -8
#define CARD_RESULT_NOPERM         -9
#define CARD_RESULT_LIMIT          -10
#define CARD_RESULT_NAMETOOLONG    -11
#define CARD_RESULT_ENCODING       -12
#define CARD_RESULT_CANCELED       -13
#define CARD_RESULT_FATAL_ERROR    -128

// Block/sector sizes
#define CARD_BLOCK_SIZE         8192    // 8KB blocks
#define CARD_ICON_WIDTH         32
#define CARD_ICON_HEIGHT        32
#define CARD_BANNER_WIDTH       96
#define CARD_BANNER_HEIGHT      32

// File attributes
#define CARD_ATTRIB_PUBLIC      0x04
#define CARD_ATTRIB_NO_MOVE     0x08
#define CARD_ATTRIB_NO_COPY     0x10
#define CARD_ATTRIB_GLOBAL      0x20

// Maximum filename length
#define CARD_FILENAME_MAX       32

/*---------------------------------------------------------------------------*
    Types
 *---------------------------------------------------------------------------*/

/**
 * @brief Memory card callback function
 */
typedef void (*CARDCallback)(s32 chan, s32 result);

/**
 * @brief File information structure
 */
typedef struct CARDFileInfo {
    s32   chan;             // Card channel
    s32   fileNo;           // File number
    s32   offset;           // Current file offset
    s32   length;           // File length
    u16   iBlock;           // Starting block number
    u16   __padding;
} CARDFileInfo;

/**
 * @brief File statistics structure
 */
typedef struct CARDStat {
    char  fileName[CARD_FILENAME_MAX];  // Filename
    u32   length;                       // File size in bytes
    u32   time;                         // Last modified time
    u8    gameName[4];                  // Game code
    u8    company[2];                   // Company code
    u8    bannerFormat;                 // Banner image format
    u8    iconAddr;                     // Icon address
    u16   iconFormat;                   // Icon format
    u16   iconSpeed;                    // Icon animation speed
    u8    permission;                   // File permissions
    u8    copyTimes;                    // Copy count
    u16   commentAddr;                  // Comment address
    u32   offsetBanner;                 // Banner offset
    u32   offsetBannerTlut;             // Banner palette offset
    u32   offsetIcon[8];                // Icon offsets (8 frames)
    u32   offsetIconTlut;               // Icon palette offset
    u32   offsetData;                   // Data offset
} CARDStat;

/*---------------------------------------------------------------------------*
    Functions
 *---------------------------------------------------------------------------*/

/**
 * @brief Initialize memory card subsystem
 */
void CARDInit(void);

/**
 * @brief Probe for memory card presence
 */
BOOL CARDProbe(s32 chan);

/**
 * @brief Probe for memory card with extended info
 */
s32 CARDProbeEx(s32 chan, s32* memSize, s32* sectorSize);

/**
 * @brief Mount memory card
 */
s32 CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback, CARDCallback attachCallback);

/**
 * @brief Mount memory card (synchronous)
 */
s32 CARDMount(s32 chan, void* workArea, CARDCallback detachCallback);

/**
 * @brief Unmount memory card
 */
s32 CARDUnmount(s32 chan);

/**
 * @brief Format memory card
 */
s32 CARDFormatAsync(s32 chan, CARDCallback callback);

/**
 * @brief Format memory card (synchronous)
 */
s32 CARDFormat(s32 chan);

/**
 * @brief Create new file
 */
s32 CARDCreateAsync(s32 chan, const char* fileName, u32 size, CARDFileInfo* fileInfo, CARDCallback callback);

/**
 * @brief Create new file (synchronous)
 */
s32 CARDCreate(s32 chan, const char* fileName, u32 size, CARDFileInfo* fileInfo);

/**
 * @brief Open existing file
 */
s32 CARDOpen(s32 chan, const char* fileName, CARDFileInfo* fileInfo);

/**
 * @brief Open file by number (fast)
 */
s32 CARDFastOpen(s32 chan, s32 fileNo, CARDFileInfo* fileInfo);

/**
 * @brief Close file
 */
s32 CARDClose(CARDFileInfo* fileInfo);

/**
 * @brief Read from file
 */
s32 CARDReadAsync(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset, CARDCallback callback);

/**
 * @brief Read from file (synchronous)
 */
s32 CARDRead(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset);

/**
 * @brief Write to file
 */
s32 CARDWriteAsync(CARDFileInfo* fileInfo, const void* buf, s32 length, s32 offset, CARDCallback callback);

/**
 * @brief Write to file (synchronous)
 */
s32 CARDWrite(CARDFileInfo* fileInfo, const void* buf, s32 length, s32 offset);

/**
 * @brief Delete file by name
 */
s32 CARDDeleteAsync(s32 chan, const char* fileName, CARDCallback callback);

/**
 * @brief Delete file by name (synchronous)
 */
s32 CARDDelete(s32 chan, const char* fileName);

/**
 * @brief Delete file by number (fast)
 */
s32 CARDFastDeleteAsync(s32 chan, s32 fileNo, CARDCallback callback);

/**
 * @brief Delete file by number (synchronous)
 */
s32 CARDFastDelete(s32 chan, s32 fileNo);

/**
 * @brief Get file statistics
 */
s32 CARDGetStatus(s32 chan, s32 fileNo, CARDStat* stat);

/**
 * @brief Set file statistics
 */
s32 CARDSetStatus(s32 chan, s32 fileNo, CARDStat* stat);

/**
 * @brief Get file statistics (from file info)
 */
s32 CARDGetStatusEx(s32 chan, const CARDFileInfo* fileInfo, CARDStat* stat);

/**
 * @brief Set file statistics (from file info)
 */
s32 CARDSetStatusEx(s32 chan, CARDFileInfo* fileInfo, CARDStat* stat);

/**
 * @brief Rename file
 */
s32 CARDRename(s32 chan, const char* oldName, const char* newName);

/**
 * @brief Get free space
 */
s32 CARDFreeBlocks(s32 chan, s32* bytesNotUsed, s32* filesNotUsed);

/**
 * @brief Check and repair card
 */
s32 CARDCheckAsync(s32 chan, CARDCallback callback);

/**
 * @brief Check and repair card (synchronous)
 */
s32 CARDCheck(s32 chan);

/**
 * @brief Check with progress tracking
 */
s32 CARDCheckExAsync(s32 chan, s32* xferBytes, CARDCallback callback);

/**
 * @brief Check with progress tracking (synchronous)
 */
s32 CARDCheckEx(s32 chan, s32* xferBytes);

/**
 * @brief Get result code from last operation
 */
s32 CARDGetResultCode(s32 chan);

/**
 * @brief Get memory card encoding
 */
s32 CARDGetEncoding(s32 chan, u16* encode);

/**
 * @brief Get memory size
 */
s32 CARDGetMemSize(s32 chan, u16* size);

/**
 * @brief Get sector size
 */
s32 CARDGetSectorSize(s32 chan, u32* size);

/**
 * @brief Set disc ID for save file verification
 */
s32 CARDSetDiskID(s32 chan, const DVDDiskID* diskID);

/**
 * @brief Set fast mode (for newer cards)
 */
BOOL CARDSetFastMode(BOOL enable);

/**
 * @brief Get fast mode status
 */
BOOL CARDGetFastMode(void);

/**
 * @brief Get current card mode
 */
s32 CARDGetCurrentMode(s32 chan, u32* mode);

/**
 * @brief Get bytes transferred in last operation
 */
s32 CARDGetXferredBytes(s32 chan);

#ifdef __cplusplus
}
#endif

#endif // DOLPHIN_CARD_H

