#include <dolphin/types.h>

#include "simulator/sim_dsp.hpp"
#include "simulator/sim_MessageQueue.hpp"

#include <SDL2/SDL.h>



namespace SIM::DSP {

static SIM::DSP::Processor sProcessor;
static SDL_Thread* sThread;
static SIM::MessageQueue sMessageQueue = SIM::MessageQueue<SIM::DSP::ThreadMessage>(256);

void Init() {
    sThread = SDL_CreateThread(MainThread, "SIM::DSP", nullptr);
}

int MainThread(void * arg) {
    while(true) {
        //Wait for messages
        auto msg = sMessageQueue.ReceiveMessage();

        switch(msg.mType) {
            case ThreadMessageType::DspMail:
                {
                    // Handle dsp mail

                } break;
            default:
                break;
        }
    }
    return 0;
}

void SendMailToDSP(u32 mail) {
    ThreadMessage msg;
    msg.mType = ThreadMessageType::DspMail;
    msg.mMail = mail;

    sMessageQueue.SendMessage(msg);
}

}

// C APIs for DSP
void SIM_DSPSendMailToDSP(u32 mail) {
    SIM::DSP::SendMailToDSP(mail);
}