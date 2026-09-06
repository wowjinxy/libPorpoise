#include "simulator/sim_dsp_BootupMicrocode.hpp"

#include <simulator/sim_dsp.hpp>

namespace SIM::DSP {

BootupMicrocode::BootupMicrocode() {
    mCurrentState = State::Ready;
}

BootupMicrocode::~BootupMicrocode() {
}

void BootupMicrocode::ReceiveMail(u32 mail) {
    switch(mCurrentState) {
        case State::Ready:
            switch(mail) {
                case 0x80F3A001:
                    mCurrentState = State::ReceiveRamMmemAddr;
                    break;
                case 0x80F3C002:
                    mCurrentState = State::ReceiveRamAddr;
                    break;
                case 0x80F3A002:
                    mCurrentState = State::ReceiveRamLength;
                    break;
                case 0x80F3B002:
                    mCurrentState = State::ReceiveAramMmemAddr;
                    break;
                case 0x80F3D001:
                    mCurrentState = State::ReceiveDspInitVector;
                    break;
                default:
                    break;
            }
            break;
        case State::ReceiveRamMmemAddr:
            mRamMmemAddr = mail;
            mRamMmemAddrSet = true;
            mCurrentState = State::Ready;
            break;
        case State::ReceiveRamAddr:
            mRamAddr = mail;
            mRamAddrSet = true;
            mCurrentState = State::Ready;
            break;
        case State::ReceiveRamLength:
            mRamLength = mail;
            mRamLengthSet = true;
            mCurrentState = State::Ready;
            break;
        case State::ReceiveAramMmemAddr:
            mAramMmemAddr = mail;
            mAramMmemAddrSet = true;
            mCurrentState = State::Ready;
            break;
        case State::ReceiveDspInitVector:
            mDspInitVector = mail;
            mDspInitVectorSet = true;
            mCurrentState = State::Ready;
            break;
    }

    // Once all values have been set, upload the new Microcode
    if(mRamMmemAddrSet
    && mRamAddrSet
    && mRamLengthSet
    && mAramMmemAddrSet
    && mDspInitVectorSet) {
        UploadMicrocode(mRamMmemAddr, mRamAddr, mRamLength, mAramMmemAddr, mDspInitVector);
    }
}

u32 BootupMicrocode::GetOutboundMail() {
    switch(mCurrentState) {
        case State::Ready:
            return 0x8071FEED;
        case State::ReceiveRamMmemAddr:
        case State::ReceiveRamAddr:
        case State::ReceiveRamLength:
        case State::ReceiveAramMmemAddr:
        case State::ReceiveDspInitVector:
            return 1;
        default:
            return 0;
    }
}

}

