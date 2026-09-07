#ifndef LIBPORPOISE_SIM_DSP_ZELDA_MICROCODE_HPP
#define LIBPORPOISE_SIM_DSP_ZELDA_MICROCODE_HPP

#include <dolphin/types.h>
#include <SDL2/SDL_mutex.h>

#include <simulator/sim_dsp_IMicrocode.hpp>
#include <simulator/sim_MessageQueue.hpp>
#include <array>
#include <vector>
#include <deque>

namespace SIM::DSP {

class ZeldaMicrocode : public IMicrocode {
    enum Flags {
        LightProtocol = 1,
        Revolution = 1 << 1, /* Wii, No ARAM */
        LouderDolby = 1 << 2,
        FourMixingOutputs = 1 << 3,
        SmallVPB = 1 << 4,
        ExplicitVolumeStep = 1 << 5,
        GBACryptoSupport = 1 << 6,
        PerFrameSync = 1 << 7,
        Command0C = 1 << 8,
        CombinedCommand0D = 1 << 9,
        NoCommand0D = 1 << 10
    };

    enum class State {
        Ready,
        ReceiveCommand,
        Rendering,
        Halted,

        Count,
    };

    struct Command {
        u32 mCommandId;
        u32 mFullCommandMail;
        u32 mSyncValue;
        u32 mExtraData;
        u32 mCommandMails[4];
    };

    public:
        ZeldaMicrocode(u32 crc);
        ~ZeldaMicrocode();
        virtual void ReceiveMail(u32 mail);
        virtual u32 GetOutboundMail();
        void OnPeriodicUpdate();
    
    private:
        void ProcessMailLight(u32 mail);
        void RunPendingCommands();
        void RunCommand(Command& cmd);
        void SendAck(u16 syncValue);
        void RenderAudio();

        State mCurrentState;
        u32 mFlags;
        u32 mOutboundMail;
        u32 mNumCommandMails;
        u32 mCommandId;
        Command mCurrentCommand;
        std::deque<Command> mPendingCommands;
        std::vector<u32> mCommandMails;

        u32 mVoicesPerFrame;
        void * mVoiceParamBaseAddress;
        void * mReverbBaseAddress;
        std::array<s16, 0x100> mMixingCoefficients{};
        std::array<s16, 0x100> mConstPatterns{};
        std::array<s16, 0x20> mAfcCoefficients{};

        u32 mRequestedFrames;
        u32 mOutputVolume;
        s16 * mOutputLeftBufferAddr;
        s16 * mOutputRightBufferAddr;
        u32 mSyncMaxVoiceId;
        std::array<u16, 256> mSyncVoiceSkipFlags{};
};

}

#endif
