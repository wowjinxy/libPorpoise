#include <dolphin.h>

#include "simulator/sim_dsp.hpp"
#include "simulator/sim_MessageQueue.hpp"
#include "simulator/sim_dsp_BootupMicrocode.hpp"
#include "simulator/sim_dsp_ZeldaMicrocode.hpp"
#include "simulator/sim_memory.hpp"
#include "simulator/sim_crc32.h"
#include "dolphin/os/OSInterrupt.h"

#include <SDL2/SDL.h>



namespace SIM::DSP {

static SDL_Thread* sThread;
static SIM::MessageQueue sMessageQueue = SIM::MessageQueue<SIM::DSP::ThreadMessage>(256);
static IMicrocode * sMicrocode;

void Init() {
    sMicrocode = new BootupMicrocode();
    sThread = SDL_CreateThread(MainThread, "SIM::DSP", nullptr);
}

int MainThread(void * arg) {
    while(true) {
        //Wait for messages
        auto msg = sMessageQueue.ReceiveMessage();

        switch(msg.mType) {
            // Handle mail sent to DSP
            case ThreadMessageType::SendMailToDSP:
                {
                    sMicrocode->ReceiveMail(msg.mSendMail);
                } break;
            // Read mail from the DSP
            case ThreadMessageType::ReadMailFromDSP:
                {
                    u32 * valuePtr = msg.mReadMail.mailValue;
                    *valuePtr = sMicrocode->GetOutboundMail();
                    SDL_SemPost(msg.mReadMail.semaphore);
                }
            // Upload new microcode to DSP
            case ThreadMessageType::LoadMicrocode:
                {

                }
            default:
                break;
        }
    }
    return 0;
}

void SendMailToDSP(u32 mail) {
    ThreadMessage msg;
    msg.mType = ThreadMessageType::SendMailToDSP;
    msg.mSendMail = mail;

    sMessageQueue.SendMessage(msg);
}

// Read the mail from DSP (note: blocking operation until the DSP responds)
u32 ReadMailFromDSP() {
    ThreadMessage msg;
    msg.mType = ThreadMessageType::ReadMailFromDSP;
    msg.mReadMail.semaphore = SDL_CreateSemaphore(0);
    u32 mailResult;
    msg.mReadMail.mailValue = &mailResult;

    sMessageQueue.SendMessage(msg);
    SDL_SemWait(msg.mReadMail.semaphore);

    return mailResult;
}

void UploadMicrocode(u32 iramMmemAddrHndl, u32 iramAddr, u32 iramLength, u32 aramMmemAddr, u32 dspInitVector) {
    u8 * microcode = (u8*)SIM::Memory::MemoryHandleToAddress(iramMmemAddrHndl);
    if(microcode) {
        u32 microcodeCRC = SIM_crc32buf(microcode, iramLength);

        switch(microcodeCRC) {
            case 0xA766829F: /* Zelda Microcode */
                delete sMicrocode;
                sMicrocode = new ZeldaMicrocode(microcodeCRC);
                break;
            default:
                OSReport("DSP: Unknown microcode!\n");
                break;
        }

        if(microcodeCRC != 0) {
            printf("Here\n");
        }
    }
}

void CallInterrupt() {
    __OSInterruptHandler dspHandler = __OSGetInterruptHandler(__OS_INTERRUPT_DSP_DSP);
    if(dspHandler) {
        dspHandler(__OS_INTERRUPT_DSP_DSP, NULL);
    }
}

}

// C APIs for DSP
void SIM_DSPSendMailToDSP(u32 mail) {
    SIM::DSP::SendMailToDSP(mail);
}

u32 SIM_DSPReadMailFromDSP() {
    return SIM::DSP::ReadMailFromDSP();
}