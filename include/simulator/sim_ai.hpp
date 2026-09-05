#ifndef LIBPORPOISE_SIM_AI_HPP
#define LIBPORPOISE_SIM_AI_HPP

#include <dolphin/types.h>
#include <SDL2/SDL_mutex.h>

#include "simulator/sim_ai.h"

namespace SIM::AI {

enum class ThreadMessageType {
 SetRegValue,
 StartDma,
 StopDma,
 Count
};

struct SetRegValue {
 u32 reg;
 u32 newVal;
 SDL_sem * semaphore;
};

struct ThreadMessage {
 ThreadMessage(){};
 ThreadMessageType mType;
 union {
    SetRegValue mSetRegValue;
 };
};

void Init();
int MainThread(void * arg);

}

#endif
