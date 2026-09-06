#ifndef LIBPORPOISE_SIM_DSP_BOOTUP_MICROCODE_HPP
#define LIBPORPOISE_SIM_DSP_BOOTUP_MICROCODE_HPP

#include <dolphin/types.h>
#include <SDL2/SDL_mutex.h>

#include <simulator/sim_dsp_IMicrocode.hpp>
#include <simulator/sim_MessageQueue.hpp>

namespace SIM::DSP {

class BootupMicrocode : public IMicrocode {
    enum class State {
        Ready,
        ReceiveRamMmemAddr,
        ReceiveRamAddr,
        ReceiveRamLength,
        ReceiveAramMmemAddr,
        ReceiveDspInitVector,

        Count,
    };

    public:
        BootupMicrocode();
        virtual ~BootupMicrocode();
        virtual void ReceiveMail(u32 mail);
        virtual u32 GetOutboundMail();
    
    private:
        State mCurrentState;

        u32 mRamMmemAddr = 0;
        bool mRamMmemAddrSet = false;
        u32 mRamAddr = 0;
        bool mRamAddrSet = false;
        u32 mRamLength = 0;
        bool mRamLengthSet = false;
        u32 mAramMmemAddr = 0;
        bool mAramMmemAddrSet = false;
        u32 mDspInitVector = 0;
        bool mDspInitVectorSet = false;
};

}

#endif
