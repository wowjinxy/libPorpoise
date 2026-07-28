#ifndef REVOLUTION_CNT_H
#define REVOLUTION_CNT_H

#include <revolution/mem/allocator.h>

BEGIN_SCOPE_EXTERN_C

#define CNT_RESULT_OK 0
#define CNT_RESULT_INVALID -1
#define CNT_RESULT_ACCESS -2
#define CNT_RESULT_ALLOC_FAILED -3
#define CNT_RESULT_NOT_FOUND -4
#define CNT_RESULT_IO_ERROR -5
#define CNT_RESULT_BAD_STATUS -6

#define CNT_SEEK_SET 0
#define CNT_SEEK_CUR 1
#define CNT_SEEK_END 2

#define CNT_TYPE_HOST 3

typedef struct CNTHandle {
    void* hostData;
    u64 titleId;
    u32 contentIndex;
    u32 type;
} CNTHandle;

typedef struct CNTFileInfo {
    CNTHandle* handle;
    void* hostData;
    u32 length;
    s32 position;
} CNTFileInfo;

typedef struct CNTDir {
    CNTHandle* handle;
    void* hostData;
    u32 location;
} CNTDir;

typedef struct CNTDirEntry {
    u32 entryNum;
    BOOL isDir;
    const char* name;
} CNTDirEntry;

void CNTInit(void);
void CNTShutdown(void);
s32 CNTInitHandle(
    u32 contentIndex,
    CNTHandle* handle,
    MEMAllocator* allocator);
s32 CNTInitHandleTitle(
    u64 titleId,
    u32 contentIndex,
    CNTHandle* handle,
    MEMAllocator* allocator);
s32 CNTReleaseHandle(CNTHandle* handle);

s32 CNTOpen(
    CNTHandle* handle,
    const char* fileName,
    CNTFileInfo* file);
s32 CNTFastOpen(
    CNTHandle* handle,
    s32 entryNumber,
    CNTFileInfo* file);
s32 CNTRead(CNTFileInfo* file, void* destination, u32 length);
s32 CNTReadWithOffset(
    CNTFileInfo* file,
    void* destination,
    u32 length,
    s32 offset);
s32 CNTSeek(CNTFileInfo* file, s32 offset, u32 origin);
s32 CNTTell(CNTFileInfo* file);
u32 CNTGetLength(CNTFileInfo* file);
s32 CNTClose(CNTFileInfo* file);

s32 CNTConvertPathToEntrynum(
    CNTHandle* handle,
    const char* fileName);
BOOL CNTEntrynumIsDir(CNTHandle* handle, s32 entryNumber);
s32 CNTChangeDir(CNTHandle* handle, const char* directoryName);
s32 CNTGetCurrentDir(CNTHandle* handle, char* path, u32 maxLength);
BOOL CNTOpenDir(
    CNTHandle* handle,
    const char* directoryName,
    CNTDir* directory);
BOOL CNTReadDir(CNTDir* directory, CNTDirEntry* entry);
BOOL CNTCloseDir(CNTDir* directory);
u32 CNTTellDir(CNTDir* directory);
void CNTSeekDir(CNTDir* directory, u32 location);
void CNTRewindDir(CNTDir* directory);

BOOL CNTHostRegisterContent(
    u64 titleId,
    u32 contentIndex,
    const char* rootDirectory);
void CNTHostClearContentRegistry(void);

END_SCOPE_EXTERN_C

#endif /* REVOLUTION_CNT_H */
