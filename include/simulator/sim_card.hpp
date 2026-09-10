#ifndef SIM_CARD_HPP
#define SIM_CARD_HPP

#include <dolphin/types.h>
#include <simulator/sim_card.h>
#include <string>

namespace SIM::CARD {
void Init();
void CreateRootPath(s32 channel);
std::string GetRootPath(s32 channel);
s32 Open(s32 chan, std::string fileName, CARDFileInfo* fileInfo);
s32 Create(s32 chan, std::string fileName, u32 size, CARDFileInfo* fileInfo);
}

#endif
