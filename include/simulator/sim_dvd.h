#ifndef SIM_DVD_H
#define SIM_DVD_H

#include <dolphin/types.h>

#ifdef __cplusplus
#include <string>

namespace SIM::DVD {
void Init();
const char * GetRootDir();
s32 ConvertPathToEntrynum(const char * path);
std::string ConvertEntrynumToPath(size_t entrynum);
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

// C APIs
void SIM_DVDInit();
const char * SIM_DVDGetRootPath();
s32 SIM_DVDConvertPathToEntrynum(const char * path);
void SIM_DVDConvertEntrynumToPath(s32 entrynum, char * buf, u32 bufLen);
u32 SIM_DVDGetMaxEntrynum();

#ifdef __cplusplus
}
#endif

#endif
