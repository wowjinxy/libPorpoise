#ifndef SIM_CARD_H
#define SIM_CARD_H

#include <dolphin/types.h>
#include <dolphin/card.h>

#ifdef __cplusplus
extern "C" {
#endif

// C APIs
void SIM_CARDMount(s32 channel);
s32 SIM_CARDOpen(s32 chan, const char* fileName, CARDFileInfo* fileInfo);
s32 SIM_CARDFastOpen(s32 chan, s32 fileNum, CARDFileInfo* fileInfo);
s32 SIM_CARDCreate(s32 chan, const char * fileName, u32 size, CARDFileInfo* fileInfo);
s32 SIM_CARDGetStatus(s32 channel, s32 fileNo, CARDStat* state);

#ifdef __cplusplus
}
#endif

#endif
