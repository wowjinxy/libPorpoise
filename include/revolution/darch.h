#ifndef REVOLUTION_DARCH_H
#define REVOLUTION_DARCH_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DARCHFileInfo {
    const char* pathName;
    const void* fileStart;
    u32 length;
} DARCHFileInfo;

u32 DARCHGetArcSize(const DARCHFileInfo* fileInfo, u32 fileInfoNum);
BOOL DARCHCreate(void* dst, u32 dstSize, const DARCHFileInfo* fileInfo, u32 fileInfoNum);

#ifdef __cplusplus
}
#endif

#endif
