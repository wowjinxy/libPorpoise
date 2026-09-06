#ifndef LIBPORPOISE_SIM_DSP_ZELDA_MICROCODE_HPP
#define LIBPORPOISE_SIM_DSP_ZELDA_MICROCODE_HPP

#include <dolphin/types.h>
#include <SDL2/SDL_mutex.h>

#include <simulator/sim_dsp_IMicrocode.hpp>
#include <simulator/sim_MessageQueue.hpp>

namespace SIM::DSP {

class ZeldaMicrocode : public IMicrocode {
    enum class State {
        Ready,

        Count,
    };

    public:
        ZeldaMicrocode();
        ~ZeldaMicrocode();
        virtual void ReceiveMail(u32 mail);
        virtual u32 GetOutboundMail();
    
    private:
        State mCurrentState;
};

}

#endif
