#ifndef LIBPORPOISE_SIM_ARAM_HPP
#define LIBPORPOISE_SIM_ARAM_HPP

#include <dolphin/types.h>
#include <SDL2/SDL_mutex.h>

#include "simulator/sim_aram.h"

namespace SIM::ARAM {

enum class ThreadMessageType {
 StartDma,
 Count
};

struct StartDmaData {
    u32 mType;
    uintptr_t mMainRamAddress;
    u32 mAramAddress; 
    u32 mLength;
    ARCallback mARCallback;
    ARQCallback mARQCallback;
    uintptr_t mARQData;
};

struct ThreadMessage {
 ThreadMessage(){};
 ThreadMessageType mType;
 StartDmaData mDmaData;
};

void Init();
int MainThread(void * arg);

}

#endif
