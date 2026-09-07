#include "simulator/sim_dsp_ZeldaMicrocode.hpp"
#include "simulator/sim_memory.hpp"

#include <simulator/sim_dsp.hpp>

#include <array>

#include <stdlib.h>

namespace SIM::DSP {

ZeldaMicrocode::ZeldaMicrocode(u32 crc) {
    switch(crc) {
        case 0xA766829F: /* Animal Crossing */
            mFlags = LightProtocol | GBACryptoSupport | NoCommand0D;
            break;
    }
    mCurrentState = State::Ready;
    printf("ZeldaState: Ready\n");
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
    printf("mail: %d\n", mail);
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
                printf("ZeldaState: ReceiveCommand\n");
            } else if(validCommand) {
                mPendingCommands.push_back(mCurrentCommand);
                RunPendingCommands();
            }
        } break;

        case State::ReceiveCommand: {
            mCurrentCommand.mCommandMails[4 - mNumCommandMails] = mail;
            mNumCommandMails--;
            if(mNumCommandMails == 0) {
                mPendingCommands.push_back(mCurrentCommand);
                // Run the command now
                mCurrentState = State::Ready;
                printf("ZeldaState: Ready\n");
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
                printf("Zelda: Calling DSP interrupt\n");
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
                mVoiceParamBaseAddress = (void*)(cmd.mCommandMails[0]);
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
                    printf("ZeldaState: Rendering\n");
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
    printf("ZeldaMicrocode::RenderAudio\n");

    while(mCurrentFrame < mRequestedFrames) {
        

        while(mCurrentVoice < mVoicesPerFrame) {

            if(mCurrentVoice >= mSyncMaxVoiceId) {
                return;
            }



            mCurrentVoice++;
        }

        // Finalize frame

        mCurrentVoice = 0;
        mSyncMaxVoiceId = 0;
        mCurrentFrame++;
    }

    for(int i=0; i < mRequestedFrames * 80; i++) {
        // for now fill the buffer up with random junk so we can hear "something"
        s16 value = rand();
        mOutputLeftBufferAddr[i] = value;
        mOutputRightBufferAddr[i] = value;
    }


    mCurrentState = State::Ready;
    printf("ZeldaState: Ready\n");
}

}

