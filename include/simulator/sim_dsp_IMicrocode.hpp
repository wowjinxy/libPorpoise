#ifndef LIBPORPOISE_SIM_DSP_IMICROCODE_HPP
#define LIBPORPOISE_SIM_DSP_IMICROCODE_HPP

#include <dolphin/types.h>
#include <SDL2/SDL_mutex.h>

#include "simulator/sim_dsp.h"

namespace SIM::DSP {

class IMicrocode {
    public:
        virtual void ReceiveMail(u32 mail) = 0;
        virtual u32 GetOutboundMail() = 0;
        virtual void OnPeriodicUpdate() = 0;
};

}

#endif
