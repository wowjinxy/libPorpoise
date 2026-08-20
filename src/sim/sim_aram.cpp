#include <dolphin/types.h>

#include "simulator/sim_aram.hpp"
#include "simulator/sim_MessageQueue.hpp"

#include <SDL2/SDL.h>

static u8 sAramBuffer[16 * 1024* 1024] = {};
static SDL_Thread* sAramThread;
static SIM::MessageQueue sAramMessageQueue = SIM::MessageQueue<SIM::ARAM::ThreadMessage>(256);

namespace SIM::ARAM {
void Init() {
    sAramThread = SDL_CreateThread(MainThread, "SIM::ARAM", nullptr);
}

int MainThread(void * arg) {
    while(true) {
        //Wait for messages
        auto msg = sAramMessageQueue.ReceiveMessage();

        switch(msg.mType) {
            case ThreadMessageType::StartDma:
                {
                    void * mainRam = (void*)(msg.mDmaData.mMainRamAddress);
                    void * aram = (void*)&sAramBuffer[msg.mDmaData.mAramAddress];
                    if(msg.mDmaData.mType == ARAM_DIR_MRAM_TO_ARAM) {
                        memcpy(aram, mainRam, msg.mDmaData.mLength);
                    } else {
                        memcpy(mainRam, aram, msg.mDmaData.mLength);
                    }

                    if(msg.mDmaData.mARQCallback) {
                        msg.mDmaData.mARQCallback(msg.mDmaData.mARQData);
                    } else if(msg.mDmaData.mARCallback) {
                        msg.mDmaData.mARCallback();
                    }
                } break;
            default:
                break;
        }
    }
    return 0;
}

}

// C APIs for ARAM
void SIM_ARAMStartDMA(u32 type, uintptr_t mainmem_addr, u32 aram_addr, u32 length, ARCallback arCallback, ARQCallback arqCallback, uintptr_t arqData) {
    SIM::ARAM::ThreadMessage msg;
    msg.mType = SIM::ARAM::ThreadMessageType::StartDma;
    msg.mDmaData.mType = type;
    msg.mDmaData.mMainRamAddress = mainmem_addr;
    msg.mDmaData.mAramAddress = aram_addr;
    msg.mDmaData.mLength = length;
    msg.mDmaData.mARCallback = arCallback;
    msg.mDmaData.mARQCallback = arqCallback;
    msg.mDmaData.mARQData = arqData;

    sAramMessageQueue.SendMessage(msg);
}