#ifndef LIBPORPOISE_SIM_DSP_ZELDA_MICROCODE_HPP
#define LIBPORPOISE_SIM_DSP_ZELDA_MICROCODE_HPP

#include <dolphin/types.h>
#include <SDL2/SDL_mutex.h>

#include <simulator/sim_dsp_IMicrocode.hpp>
#include <simulator/sim_MessageQueue.hpp>

namespace SIM::DSP {

class ZeldaMicrocode : public IMicrocode {
    enum Flags {
        LightProtocol = 0,
        Revolution = 1, /* Wii, No ARAM */
        LouderDolby = 1 << 1,
        FourMixingOutputs = 1 << 2,
        SmallVPB = 1 << 3,
        ExplicitVolumeStep = 1 << 4,
        GBACryptoSupport = 1 << 5,
        PerFrameSync = 1 << 6,
        Command0C = 1 << 7,
        CombinedCommand0D = 1 << 8,
        NoCommand0D = 1 << 9
    };


    enum class State {
        Ready,

        Count,
    };

    public:
        ZeldaMicrocode(u32 crc);
        ~ZeldaMicrocode();
        virtual void ReceiveMail(u32 mail);
        virtual u32 GetOutboundMail();
    
    private:
        State mCurrentState;
        u32 mFlags;
};

}

#endif
