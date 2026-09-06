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

typedef union {
    u32 raw;
    struct {
        u32 playingStatus:1;
        u32 auxFrequency:1;
        u32 interruptMask:1;
        u32 interruptStatus:1;
        u32 interruptValid:1;
        u32 sampleCounterReset:1;
        u32 dspSampleRate:1;
        u32 unused:25;
    };
} ControlRegister;

typedef union {
    u32 raw;
    struct {
        u8 left;
        u8 right;
        u16 unused; 
    };
} VolumeRegister;

void Init();
int MainThread(void * arg);

}

#endif
