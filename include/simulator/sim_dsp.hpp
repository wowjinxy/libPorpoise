#ifndef LIBPORPOISE_SIM_DSP_HPP
#define LIBPORPOISE_SIM_DSP_HPP

#include <dolphin/types.h>
#include <SDL2/SDL_mutex.h>

#include "simulator/sim_dsp.h"

#include "simulator/sim_dsp_IMicrocode.hpp"

namespace SIM::DSP {

enum class ThreadMessageType {
 DspMail,
 Count
};

struct ThreadMessage {
 ThreadMessage(){};
 ThreadMessageType mType;
 u32 mMail;
};

class Processor {
 public:
    Processor() {};

 private:
    IMicrocode * mMicrocode;
};

void Init();
int MainThread(void * arg);
void SendMailToDSP(u32 mail);

}

#endif
