#include <dolphin/types.h>
#include <dolphin/hw_regs.h>

#include "simulator/sim_ai.hpp"
#include "simulator/sim_MessageQueue.hpp"

#include <SDL2/SDL.h>

namespace SIM::AI {
static SDL_Thread* sAiThread;
static SIM::MessageQueue sMessageQueue = SIM::MessageQueue<SIM::AI::ThreadMessage>(256);


void Init() {
    sAiThread = SDL_CreateThread(MainThread, "SIM::AI", nullptr);
}

int MainThread(void * arg) {
    while(true) {
        //Wait for messages
        auto msg = sMessageQueue.ReceiveMessage();

        switch(msg.mType) {
            case ThreadMessageType::SetRegValue:
                {
                    //SET_REG_FIELD(0, __AIRegs[msg.mSetRegField.reg], msg.mSetRegField.size, msg.mSetRegField.shift);
                    //TODO: This is where we will compare the new value to the old and perform any necessary actions

                    __AIRegs[msg.mSetRegValue.reg] = msg.mSetRegValue.newVal;
                    SDL_SemPost(msg.mSetRegValue.semaphore);
                } break;
            case ThreadMessageType::StartDma:
                {

                } break;
            default:
                break;
        }
    }
    return 0;
}

}

// C APIs for AI
void SIM_AISetRegValue(u32 reg, u32 newVal) {
    SIM::AI::ThreadMessage msg;
    msg.mType = SIM::AI::ThreadMessageType::SetRegValue;
    msg.mSetRegValue.reg = reg;
    msg.mSetRegValue.newVal = newVal;
    msg.mSetRegValue.semaphore = SDL_CreateSemaphore(0);

    SIM::AI::sMessageQueue.SendMessage(msg);

    // This is a blocking operation since it is the equivalent of doing __AIRegs[reg] = newVal
    // When this function completes, the AI reg should be set.
    // AI thread will post to the semaphore once it has processed the request.
    SDL_SemWait(msg.mSetRegValue.semaphore);
    SDL_DestroySemaphore(msg.mSetRegValue.semaphore);
}