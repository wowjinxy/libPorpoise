#include "simulator/sim_dsp_ZeldaMicrocode.hpp"
#include "simulator/sim_memory.hpp"

#include <simulator/sim_dsp.hpp>
#include <simulator/sim_aram.hpp>
#include <simulator/byteswap.h>

#include <algorithm>
#include <array>
#include <string.h>

#include <stdlib.h>

namespace SIM::DSP {

ZeldaMicrocode::ZeldaMicrocode(u32 crc) {
    switch(crc) {
        case 0xA766829F: /* Animal Crossing */
            mFlags = LightProtocol | GBACryptoSupport | NoCommand0D;
            break;
    }
    mCurrentState = State::Ready;
    //printf("ZeldaState: Ready\n");
    mOutboundMail = 0x88881111;
    SetMailboxFull();
    CallInterrupt();
}

ZeldaMicrocode::~ZeldaMicrocode() {
}

void ZeldaMicrocode::ReceiveMail(u32 mail) {
    if(mFlags & LightProtocol) {
        ProcessMailLight(mail);
    } else {
        // Non-light protocol not implemented yet!
    }

}

void ZeldaMicrocode::ProcessMailLight(u32 mail) {
    //printf("mail: %d\n", mail);
    switch(mCurrentState) {
        case State::Ready: {
            bool validCommand = true;
            mCurrentCommand.mCommandId = (mail >> 24) & 0x7F;
            mCurrentCommand.mFullCommandMail = mail;
            mCurrentCommand.mSyncValue = (mail >> 16);
            mCurrentCommand.mExtraData = (mail & 0xFFFF);
            switch(mCurrentCommand.mCommandId) {
                case 0x00:
                    mNumCommandMails = 0;
                    break;
                case 0x01:
                    mNumCommandMails = 4;
                    break;
                case 0x02:
                    mNumCommandMails = 2;
                    break;
                case 0x03:
                    validCommand = false;
                    break;
                case 0x0C:
                    if(mFlags & GBACryptoSupport) {
                        mNumCommandMails = 1;
                    } else if(mFlags & Command0C) {
                        mNumCommandMails = 2;
                    } else {
                        mNumCommandMails = 0;
                    }
                    break;
                default:
                    mNumCommandMails = 0;
                    break;
            }

            if(mNumCommandMails) {
                mCurrentState = State::ReceiveCommand;
                mCurrentCommandMail = 0;
                //printf("ZeldaState: ReceiveCommand\n");
            } else if(validCommand) {
                mPendingCommands.push_back(mCurrentCommand);
                RunPendingCommands();
            }
        } break;

        case State::ReceiveCommand: {
            mCurrentCommand.mCommandMails[mCurrentCommandMail] = mail;
            mCurrentCommandMail++;
            mNumCommandMails--;
            if(mNumCommandMails == 0) {
                mPendingCommands.push_back(mCurrentCommand);
                // Run the command now
                mCurrentState = State::Ready;
                //printf("ZeldaState: Ready\n");
                RunPendingCommands();
            }
        } break;

        case State::Rendering: {
            if(mFlags & PerFrameSync) {
                // Not implemented yet
            } else {
                mSyncMaxVoiceId = 0xFFFFFFFF;
                mSyncVoiceSkipFlags.fill(0xFFFF);
                RenderAudio();
                // Generate DSP interrupt
                //printf("Zelda: Calling DSP interrupt\n");
                SIM::DSP::CallInterrupt();
            }
        } break;

        case State::Halted: {
            // Should not receive mail here.
        } break;

        default:
            break;
    }
}

u32 ZeldaMicrocode::GetOutboundMail() {
    switch(mCurrentState) {
        case State::Ready:
            return mOutboundMail;
        default:
            return 0;
    }
}

void ZeldaMicrocode::OnPeriodicUpdate() {
    //CallInterrupt();
}

void ZeldaMicrocode::RunPendingCommands() {
    while(!mPendingCommands.empty()) {
        auto cmd = mPendingCommands.front();
        mPendingCommands.pop_front();
        RunCommand(cmd);

        if(mCurrentState == State::Rendering) {
            break;
        }
    }
}

void ZeldaMicrocode::RunCommand(Command& cmd) {
    switch(cmd.mCommandId) {
        // NOP commands
        case 0x00:
        case 0x0A:
        case 0x0B:
        case 0x0F:
        case 0x03:
            SendAck(cmd.mSyncValue);
            break;
        
        // These commands cause crashes on non-light protocols
        // NOP on light protocol
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
            if(!(mFlags & LightProtocol)) {
                mCurrentState = State::Halted;
                printf("ZeldaState: Halted\n");
            }
            break;
        
        // Setup/initialization
        case 0x01:
            {
                mVoicesPerFrame = cmd.mExtraData;
                mVoiceParams = (VoiceParamBase*)(cmd.mCommandMails[0]);
                s16 * mixingCoefficientsAddress = (s16*)(cmd.mCommandMails[1]);

                if(mixingCoefficientsAddress) {
                    memcpy(mMixingCoefficients.data(), mixingCoefficientsAddress, sizeof(mMixingCoefficients));
                }

                if(mixingCoefficientsAddress) {
                    memcpy(mConstPatterns.data(), mixingCoefficientsAddress + 0x100, sizeof(mConstPatterns));
                }

                if(cmd.mCommandMails[2] != 0) {
                    memcpy(mAfcCoefficients.data(), (void*)(cmd.mCommandMails[2]), sizeof(mAfcCoefficients));
                }


                mReverbBaseAddress = (void*)(cmd.mCommandMails[3]);

                SendAck(cmd.mSyncValue);
            } break;
        
        // Start audio processing
        case 0x02:
            {
                mRequestedFrames = (cmd.mFullCommandMail >> 16) & 0xFF;
                mOutputVolume = (cmd.mFullCommandMail & 0xFFFF);
                mOutputLeftBufferAddr = (s16*)(cmd.mCommandMails[0]);
                mOutputRightBufferAddr = (s16*)(cmd.mCommandMails[1]);

                mCurrentVoice = 0;
                mCurrentFrame = 0;

                if(mFlags & LightProtocol) {
                    SendAck(mRequestedFrames);

                    mCurrentState = State::Rendering;
                    //printf("ZeldaState: Rendering\n");
                } else {
                    // Not implemented
                }
            } break;
        
        
        // GBA Crypto
        case 0x0C:
            {

                SendAck(cmd.mSyncValue);
            } break;

        case 0x0D:
            {

                SendAck(cmd.mSyncValue);
            } break;

        // Set base address of ARAM (Wii only)
        case 0x0E:
            {

                SendAck(cmd.mSyncValue);
            } break;
    }


}

void ZeldaMicrocode::SendAck(u16 syncValue) {
    if(mFlags & LightProtocol) {
        syncValue = 2 * ((syncValue >> 8) & 0x7F) + 0x62;
        mOutboundMail = 0x80000000 | syncValue;
        SetMailboxFull();
    } else {
        // Non-light not implemented
    }
}

void ZeldaMicrocode::RenderAudio() {
    //printf("ZeldaMicrocode::RenderAudio\n");

    while(mCurrentFrame < mRequestedFrames) {
    
        if(mCurrentVoice == 0) {
            PrepareFrame();
        }

        while(mCurrentVoice < mVoicesPerFrame) {

            if(mCurrentVoice >= mSyncMaxVoiceId) {
                return;
            }

            u16 flags = mSyncVoiceSkipFlags[mCurrentVoice >> 4];
            u8 bit = 0xF - (mCurrentVoice & 0xF);
            if(flags & (1 << bit)) {
                RenderVoice(mCurrentVoice);
            }

            mCurrentVoice++;
        }

        FinalizeFrame();

        mCurrentVoice = 0;
        mSyncMaxVoiceId = 0;
        mCurrentFrame++;
    }

    //for(int i=0; i < mRequestedFrames * 80; i++) {
    //    // for now fill the buffer up with random junk so we can hear "something"
    //    s16 value = rand();
    //    mOutputLeftBufferAddr[i] = value;
    //    mOutputRightBufferAddr[i] = value;
    //}


    mCurrentState = State::Ready;
    //printf("ZeldaState: Ready\n");
}

void ZeldaMicrocode::PrepareFrame() {
    if(mFramePrepared) {
        return;
    }

    mFrontLeftMixBuffer.fill(0);
    mFrontRightMixBuffer.fill(0);

    // TODO apply volume in place

    // this is where we would do the reverb

    mFramePrepared = true;
}

void ZeldaMicrocode::RenderVoice(u16 voice) {
    auto& vpb = mVoiceParams[voice];

    if(!vpb.enabled || vpb.done) {
        return;
    }

    MixBuffer samplesIn;
    LoadSamplesFromVPB(vpb, samplesIn);

    // apply low pass

    // apply biquad

    if(vpb.useDolbyVolume) {
        // TODO
    } else {
        int numChannels = (mFlags & FourMixingOutputs) ? 4 : 6;
        if(vpb.endRequested) {
            bool allMute = true;
            for(int i=0; i < numChannels; ++i) {
                vpb.mixChannels[i].targetVolume = vpb.mixChannels[i].currentVolume / 2;
                allMute &= (vpb.mixChannels[i].targetVolume == 0);
            }
            if(allMute) {
                vpb.done = true;
            }
        }

        for(int i=0; i < numChannels; ++i) {
            if(!vpb.mixChannels[i].id) {
                continue;
            }

            s16 volumeDelta;
            if(mFlags & ExplicitVolumeStep) {
                volumeDelta = vpb.mixChannels[i].targetVolume;
            } else {
                volumeDelta = vpb.mixChannels[i].targetVolume - vpb.mixChannels[i].currentVolume;
            }

            s32 volumeStep = (volumeDelta << 16) / (s32)(samplesIn.size());

            if(vpb.mixChannels[i].currentVolume && !volumeStep) {
                continue;
            }

            MixBuffer * destBuffer = GetMixBufferFromChannelID(vpb.mixChannels[i].id);
            if(!destBuffer) {
                continue;
            }

            s32 newVolume = MixBuffersWithVolumeStepped(destBuffer, samplesIn, vpb.mixChannels[i].currentVolume << 16, volumeStep);
            vpb.mixChannels[i].currentVolume = newVolume >> 16;
        }
    }

    if(!vpb.useConstantSample) {
        vpb.resetVpb = false;
    }

    // TODO: store the VPB

}

void ZeldaMicrocode::FinalizeFrame() {
    //Apply volume in place

    memcpy(mOutputLeftBufferAddr, mFrontLeftMixBuffer.data(), sizeof(mFrontLeftMixBuffer));
    memcpy(mOutputRightBufferAddr, mFrontRightMixBuffer.data(), sizeof(mFrontRightMixBuffer));

    mOutputLeftBufferAddr = (s16*)((u64)mOutputLeftBufferAddr + sizeof(mFrontLeftMixBuffer));
    mOutputRightBufferAddr = (s16*)((u64)mOutputRightBufferAddr + sizeof(mFrontRightMixBuffer));

    //Apply reverb

    mFramePrepared = false;
}

u16 ZeldaMicrocode::RequiredRawSamplesCount(VoiceParamBase& vpb) {
    return (vpb.currentPosFrac + 0x50 * vpb.resamplingRatio) >> 12;
}

void ZeldaMicrocode::Resample(VoiceParamBase& vpb, const s16* source, MixBuffer& dest) {
    u32 ratio = vpb.resamplingRatio;
    u32 pos = vpb.currentPosFrac;

    if((ratio >> 12) >= 4) {
        for (s16& destSample : dest)
        {
          pos += ratio;
          destSample = source[pos >> 12];
        }
    } else {
        for(auto& destSample : dest) {
            u32 coeffIndex = ((pos & 0xFFF) >> 6) * 4;
            const s16* coeffs = &mResamplingCoefficients[coeffIndex];
            const s16* input = &source[pos >> 12];

            s64 destSampleUnclamped = 0;
            for(u32 i=0; i < 4; ++i) {
                destSampleUnclamped += (s64)2 * coeffs[i] * input[i];
            }
            destSampleUnclamped >>= 16;

            destSample = (s16)std::clamp<s64>(destSampleUnclamped, -0x8000, 0x7FFF);

            pos += ratio;
        }
    }

    for(u32 i=0; i < 4; ++i) {
        vpb.resampleBuffer[i] = source[(pos >> 12) + i];
    }
    vpb.constantSample = dest[dest.size() - 1];
    vpb.currentPosFrac = pos & 0xFFF;
}

void ZeldaMicrocode::LoadSamplesFromVPB(VoiceParamBase& vpb, MixBuffer& bufOut) {
    std::array<s16, 0x500 + 4> rawSamples;


    for(size_t i=0; i < 4; ++i) {
        rawSamples[i] = vpb.resampleBuffer[i];
    }

    if(vpb.useConstantSample) {
        bufOut.fill(vpb.constantSample);
        return;
    }

    switch(static_cast<SamplesSourceType>(vpb.samplesSourceType)) {
        case SamplesSourceType::SquareWave:
        case SamplesSourceType::SquareWave25Pct:
            break;
        case SamplesSourceType::SawWave:
            break;
        case SamplesSourceType::ConstPattern0:
        case SamplesSourceType::ConstPattern0VariableStep:
        case SamplesSourceType::ConstPattern1:
        case SamplesSourceType::ConstPattern2:
        case SamplesSourceType::ConstPattern3:
            break;
        case SamplesSourceType::Pcm8FromAram:
            CopyPCMSamplesFromARAM<s8>(rawSamples.data() + 4, vpb, RequiredRawSamplesCount(vpb));
            Resample(vpb, rawSamples.data(), bufOut);
            break;
        case SamplesSourceType::AfcLoQualityFromAram:
        case SamplesSourceType::AfcHiQualityFromAram:
            break;
        case SamplesSourceType::Pcm16FromAram:
            CopyPCMSamplesFromARAM<s16>(rawSamples.data() + 4, vpb, RequiredRawSamplesCount(vpb));
            Resample(vpb, rawSamples.data(), bufOut);
            break;
        case SamplesSourceType::Pcm16FromMram:
            CopyPCMSamplesFromMRAM(rawSamples.data() + 4, vpb, RequiredRawSamplesCount(vpb));
            Resample(vpb, rawSamples.data(), bufOut);
            break;
        default:
            // Invalid source
            bufOut.fill(0);
            return;
    }
}

template <typename T>
void ZeldaMicrocode::CopyPCMSamplesFromARAM(s16 * dest, VoiceParamBase& vpb, u16 sampleCount) {
    if(vpb.done) {
        for(u16 i=0; i < sampleCount; ++i) {
            dest[i] = 0;
        }
        return;
    }

    if(vpb.resetVpb) {
        vpb.remainingLength = vpb.loopStartPosition - vpb.currentPosition;
        vpb.currentAramAddr = vpb.baseAddress + (vpb.currentPosition * sizeof(T));
    }

    vpb.endReached = false;
    while(sampleCount > 0) {
        if(vpb.endReached) {
            vpb.endReached = false;
            if(!vpb.isLooping) {
                for(u16 i = 0; i < sampleCount; ++i) {
                    dest[i] = 0;
                }
                vpb.done = true;
                break;
            }
            vpb.currentPosition = vpb.loopAddress;
            vpb.remainingLength = vpb.loopStartPosition - vpb.currentPosition;
            vpb.currentAramAddr = vpb.baseAddress + (vpb.currentPosition * sizeof(T));
        }

        u16 samplesToCopy = std::min<u16>(vpb.remainingLength, sampleCount);
        T* sourcePtr = reinterpret_cast<T*>(SIM::ARAM::GetAramPointer(vpb.currentAramAddr));

        for(u16 i=0; i < samplesToCopy; ++i) {
            if(sizeof(T) == 2) {
                *dest++ = bswap_16(*sourcePtr++) << (16 - 8 * sizeof(T));
            } else {
                *dest++ = (*sourcePtr++) << (16 - 8 * sizeof(T));
            }
        }

        vpb.remainingLength = vpb.remainingLength - samplesToCopy;
        vpb.currentAramAddr = vpb.currentAramAddr + (samplesToCopy * sizeof(T));
        sampleCount -= samplesToCopy;

        if(!vpb.remainingLength) {
            vpb.endReached = true;
        }
    }
}

void ZeldaMicrocode::CopyPCMSamplesFromMRAM(s16 * dest, VoiceParamBase& vpb, u16 sampleCount) {
    s16 * addr = (s16*)(vpb.baseAddress + ((vpb.currentPosition & 0xFFFF0000) >> 16) * sizeof(s16));

    u32 remainingLength = vpb.remainingLength;

    if(sampleCount > remainingLength) {
        s16 lastSample = 0;
        if(remainingLength != 0) {
            memcpy(dest, addr, remainingLength * sizeof(s16));
            lastSample = dest[remainingLength - 1];
        }
        for(u16 i=remainingLength; i < sampleCount; ++i) {
            *dest++ = lastSample;
        }

        u32 currentPositionHigh = (vpb.currentPosition & 0xFFFF0000) >> 16;
        vpb.currentPosition &= 0x0000FFFF;
        currentPositionHigh += remainingLength;
        vpb.currentPosition |= (currentPositionHigh << 16);
        vpb.remainingLength = 0;
        vpb.done = true;
    } else {
        vpb.remainingLength = remainingLength - sampleCount;
        vpb.samplesBeforeLoop = ((vpb.loopStartPosition & 0xFFFF0000) >> 16) - ((vpb.currentPosition & 0xFFFF0000) >> 16);
        if(sampleCount <= vpb.samplesBeforeLoop) {
            memcpy(dest, addr, sampleCount * sizeof(s16));
            u32 currentPositionHigh = (vpb.currentPosition & 0xFFFF0000) >> 16;
            vpb.currentPosition &= 0x0000FFFF;
            currentPositionHigh += sampleCount;
            vpb.currentPosition |= (currentPositionHigh << 16);
        } else {
            memcpy(dest, addr, vpb.samplesBeforeLoop * sizeof(s16));
            vpb.baseAddress = vpb.loopAddress;
            vpb.currentPosition &= 0x0000FFFF;
            vpb.currentPosition |= ((sampleCount - vpb.samplesBeforeLoop) << 16);
            memcpy(dest + vpb.samplesBeforeLoop, (void*)vpb.loopAddress, ((vpb.currentPosition & 0xFFFF0000) >> 16) * sizeof(s16));
        }
    }
}

ZeldaMicrocode::MixBuffer * ZeldaMicrocode::GetMixBufferFromChannelID(int id) {
    switch(id) {
        case 0x0D00:
            return &mFrontLeftMixBuffer;
        case 0x0D60:
            return &mFrontRightMixBuffer;
        case 0x0F40:
            return &mBackLeftMixBuffer;
        case 0x0CA0:
            return &mBackRightMixBuffer;
        case 0x0E80:
            return &mFrontLeftReverbBuffer;
        case 0x0EE0:
            return &mFrontRightReverbBuffer;
        case 0x0C00:
            return &mBackLeftReverbBuffer;
        case 0x0C50:
            return &mBackRightReverbBuffer;
        default:
            return nullptr;
    }
}

template <size_t N>
s32 ZeldaMicrocode::MixBuffersWithVolumeStepped(std::array<s16, N> * dest, const std::array<s16, N>& source, s32 volume, s32 step) {
    if (!volume && !step)
      return volume;

    for (size_t i = 0; i < N; ++i)
    {
      (*dest)[i] += ((volume >> 16) * source[i]) >> 16;
      volume += step;
    }

    return volume;
}

}

