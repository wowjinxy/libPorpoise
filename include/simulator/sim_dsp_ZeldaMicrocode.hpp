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

    enum class SamplesSourceType {
        SquareWave = 0,
        SawWave = 1,
        SquareWave25Pct = 3,
        ConstPattern1 = 4,
        AfcLoQualityFromAram = 5,
        ConstPattern0 = 7,
        Pcm8FromAram = 8,
        AfcHiQualityFromAram = 9,
        ConstPattern0VariableStep = 10,
        ConstPattern2 = 11,
        ConstPattern3 = 12,
        Pcm16FromAram = 16,
        Pcm16FromMram = 33,
    };

    struct Command {
        u32 mCommandId;
        u32 mFullCommandMail;
        u32 mSyncValue;
        u32 mExtraData;
        u32 mCommandMails[4];
    };

    typedef std::array<s16, 80> MixBuffer;

    /* Based on DSPChannel from Animal Crossing */
    #pragma pack(push, 1)
    struct VoiceParamBase {
	    u16 enabled;                    // _00 - DSP_AllocInit, DSP_PlayStop, DSP_PlayStart
	    u16 done;                       // _02 - DSP_AllocInit
	    u16 resamplingRatio;            // _04 - DSP_SetPitch
	    u16 _06;                        // _06
	    u16 resetVpb;                   // _08 - DSP_PlayStart
	    u16 endReached;                 // _0A
	    u16 useConstantSample;          // _0C - DSP_SetPauseFlag
	    u16 samplesToKeepCount;         // _0E - DSP_SetMixerInitDelayMax
        struct MixerChannel {
	        u16 id;                     // _00
	        u16 targetVolume;           // _02
	        u16 currentVolume;          // _04
	        u16 level;                  // _06
        } mixChannels[6];               // _10 - DSP_SetMixerInitVolume, DSP_SetMixerVolume, DSP_SetBusConnect
	    u8 _40[0x50 - 0x40];            // _40
	    u16 dolbyVoicePosition;         // _50
	    s16 dolbyReverbFactor;          // _52
	    s16 dolbyVolumeCurrent;         // _54
	    s16 dolbyVolumeTarget;          // _56
	    u16 useDolbyVolume;             // _58
	    u16 _5A;                        // _5A
	    u16 _5C;                        // _5C
	    u16 _5E;                        // _5E
	    u16 currentPosFrac;             // _60 - DSP_PlayStart
	    u16 _62;                        // _62
	    u16 afcRemainingDecodedSamples; // _64 - DSP_SetOscInfo, DSP_SetWaveInfo
	    s16 constantSample;             // _66 - DSP_PlayStart
	    u32 currentPosition;            // _68 - DSP_PlayStart
	    u16 samplesBeforeLoop;          // _6C
	    u16 _6E;                        // _6E
	    u32 currentAramAddr;            // _70
	    u32 remainingLength;            // _74
	    s16 resampleBuffer[4];          // _78 - DSP_PlayStart
	    u16 variableFirHistory[20];     // _80 - DSP_PlayStart
	    s16 biquadHistory[4];           // _A8 - DSP_PlayStart
	    u16 afcRemainingSamples[16];    // _B0 - DSP_SetWaveInfo
	    s16 lowPassHistory[2];          // _D0
	    u8 _D4[0x100 - 0xD4];           // _D4
	    u16 samplesSourceType;          // _100 - DSP_SetOscInfo, DSP_SetWaveInfo
	    u16 isLooping;              // _102 - DSP_SetWaveInfo
	    s16 loopYN1;                    // _104 - DSP_SetWaveInfo
	    s16 loopYN2;                    // _106 - DSP_SetWaveInfo
	    s16 filterMode;                 // _108 - DSP_SetFilterMode
	    u16 endRequested;           // _10A - DSP_AllocInit, DSP_SetMixerVolume
	    u32 _10C;                       // _10C - DSP_PlayStart
	    u32 loopAddress;                // _110 - DSP_SetWaveInfo
	    u32 loopStartPosition;          // _114 - DSP_SetWaveInfo
	    u32 baseAddress;                // _118 - DSP_SetOscInfo, DSP_SetWaveInfo
	    u32 _11C;                       // _11C - DSP_SetWaveInfo
	    s16 variableFirCoeffs[20];      // _120 - DSP_InitFilter, DSP_SetFIR8FilterParam
	    s16 biquadFilterCoeffs[4];      // _148 - DSP_InitFilter, DSP_SetIIRFilterParam
	    s16 lowPassCoeff;               // _150 - DSP_InitFilter, DSP_SetDistFilter
	    u8 padding[0x180 - 0x152];      // _152
    };
    #pragma pack(pop)

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
        void PrepareFrame();
        void FinalizeFrame();
        void RenderVoice(u16 voice);
        void LoadSamplesFromVPB(VoiceParamBase& vpb, MixBuffer& bufOut);
        u16 RequiredRawSamplesCount(VoiceParamBase& vpb);
        void Resample(VoiceParamBase& vpb, const s16* source, MixBuffer& dest);

        template <typename T>
        void CopyPCMSamplesFromARAM(s16 * dest, VoiceParamBase& vpb, u16 sampleCount);
        void CopyPCMSamplesFromMRAM(s16 * dest, VoiceParamBase& vpb, u16 sampleCount);
        MixBuffer * GetMixBufferFromChannelID(int id);

        template <size_t N>
        s32 MixBuffersWithVolumeStepped(std::array<s16, N>* dest, const std::array<s16, N>& source, s32 volume, s32 step);

        State mCurrentState;
        u32 mFlags;
        u32 mOutboundMail;
        u32 mNumCommandMails;
        u32 mCurrentCommandMail;
        u32 mCommandId;
        Command mCurrentCommand;
        std::deque<Command> mPendingCommands;
        std::vector<u32> mCommandMails;

        u32 mVoicesPerFrame;
        VoiceParamBase * mVoiceParams;
        void * mReverbBaseAddress;
        std::array<s16, 0x100> mMixingCoefficients{};
        std::array<s16, 0x100> mConstPatterns{};
        std::array<s16, 0x20> mAfcCoefficients{};
        std::array<s16, 0x100> mResamplingCoefficients{};

        u32 mRequestedFrames;
        u32 mOutputVolume;
        s16 * mOutputLeftBufferAddr;
        s16 * mOutputRightBufferAddr;
        u32 mSyncMaxVoiceId;
        std::array<u16, 256> mSyncVoiceSkipFlags{};

        u32 mCurrentVoice;
        u32 mCurrentFrame;
        bool mFramePrepared = false;


        MixBuffer mFrontLeftMixBuffer{};
        MixBuffer mFrontRightMixBuffer{};
        MixBuffer mBackLeftMixBuffer{};
        MixBuffer mBackRightMixBuffer{};
        MixBuffer mFrontLeftReverbBuffer{};
        MixBuffer mFrontRightReverbBuffer{};
        MixBuffer mBackLeftReverbBuffer{};
        MixBuffer mBackRightReverbBuffer{};

};

}

#endif
