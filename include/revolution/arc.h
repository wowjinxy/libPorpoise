#ifndef REVOLUTION_ARC_H
#define REVOLUTION_ARC_H

#include <revolution/types.h>

BEGIN_SCOPE_EXTERN_C

#define DARCH_MAGIC 0x55AA382DUL

typedef struct ARCHeader {
    u32 magic;
    u32 fstStart;
    u32 fstSize;
    u32 fileStart;
    u32 reserved[4];
} ARCHeader;

typedef struct ARCHandle {
    void* archiveStartAddr;
    void* FSTStart;
    void* fileStart;
    u32 entryNum;
    char* FSTStringStart;
    u32 FSTLength;
    u32 currDir;
} ARCHandle;

typedef struct ARCFileInfo {
    ARCHandle* handle;
    u32 startOffset;
    u32 length;
} ARCFileInfo;

typedef struct ARCDir {
    ARCHandle* handle;
    u32 entryNum;
    u32 location;
    u32 next;
} ARCDir;

typedef struct ARCDirEntry {
    ARCHandle* handle;
    u32 entryNum;
    BOOL isDir;
    char* name;
} ARCDirEntry;

BOOL ARCInitHandle(void* archiveStart, ARCHandle* handle);
BOOL ARCOpen(ARCHandle* handle, const char* fileName, ARCFileInfo* fileInfo);
BOOL ARCFastOpen(ARCHandle* handle, s32 entryNumber, ARCFileInfo* fileInfo);
s32 ARCConvertPathToEntrynum(ARCHandle* handle, const char* path);
BOOL ARCEntrynumIsDir(const ARCHandle* handle, s32 entryNumber);
BOOL ARCGetCurrentDir(ARCHandle* handle, char* path, u32 maxLength);
void* ARCGetStartAddrInMem(ARCFileInfo* fileInfo);
u32 ARCGetStartOffset(ARCFileInfo* fileInfo);
u32 ARCGetLength(ARCFileInfo* fileInfo);
BOOL ARCClose(ARCFileInfo* fileInfo);
BOOL ARCChangeDir(ARCHandle* handle, const char* directoryName);
BOOL ARCOpenDir(ARCHandle* handle, const char* directoryName, ARCDir* directory);
BOOL ARCReadDir(ARCDir* directory, ARCDirEntry* entry);
BOOL ARCCloseDir(ARCDir* directory);

END_SCOPE_EXTERN_C

#endif /* REVOLUTION_ARC_H */
