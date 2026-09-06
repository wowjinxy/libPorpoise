#include "simulator/sim_dsp_ZeldaMicrocode.hpp"

#include <simulator/sim_dsp.hpp>

namespace SIM::DSP {

ZeldaMicrocode::ZeldaMicrocode(u32 crc) {
    switch(crc) {
        case 0xA766829F: /* Animal Crossing */
            mFlags = LightProtocol | GBACryptoSupport | NoCommand0D;
            break;
    }
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
            return 0x88881111;
        default:
            return 0;
    }
}

}

