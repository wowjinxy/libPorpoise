#include "simulator/sim_dsp_ZeldaMicrocode.hpp"

#include <simulator/sim_dsp.hpp>

namespace SIM::DSP {

ZeldaMicrocode::ZeldaMicrocode() {
    mCurrentState = State::Ready;
}

ZeldaMicrocode::~ZeldaMicrocode() {
}

void ZeldaMicrocode::ReceiveMail(u32 mail) {
    switch(mCurrentState) {
        case State::Ready:
            break;

        default:
            break;

    }
}

u32 ZeldaMicrocode::GetOutboundMail() {
    switch(mCurrentState) {
        case State::Ready:
            return 0;
        default:
            return 0;
    }
}

}

