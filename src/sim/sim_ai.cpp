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

typedef union {
    u32 raw;
    struct {
        u32 playingStatus:1;
        u32 auxFrequency:1;
        u32 interruptMask:1;
        u32 interruptStatus:1;
        u32 interruptValid:1;
        u32 sampleCounterReset:1;
        u32 dspSampleRate:1;
        u32 unused:25;
    };
} ControlRegister;

typedef union {
    u32 raw;
    struct {
        u8 left;
        u8 right;
        u16 unused; 
    };
} VolumeRegister;

int MainThread(void * arg) {
    while(true) {
        //Wait for messages
        auto msg = sMessageQueue.ReceiveMessage();

        switch(msg.mType) {
            case ThreadMessageType::SetRegValue:
                {
                    //SET_REG_FIELD(0, __AIRegs[msg.mSetRegField.reg], msg.mSetRegField.size, msg.mSetRegField.shift);
                    //TODO: This is where we will compare the new value to the old and perform any necessary actions

                    auto reg = msg.mSetRegValue.reg;
                    auto newVal = msg.mSetRegValue.newVal;
                    u32 oldVal = __AIRegs[reg];

                    switch(reg) {
                        // AI Control Register
                        case 0:
                        {
                            ControlRegister bitfield;
                            bitfield.raw = newVal;
                            ControlRegister oldBitfield;
                            oldBitfield.raw = oldVal;

                            if(bitfield.playingStatus != oldBitfield.playingStatus) {
                                if(bitfield.playingStatus) {
                                    // Stream start play
                                } else {
                                    // Stream stop play
                                }
                            }

                            if(bitfield.auxFrequency != oldBitfield.auxFrequency) {
                                // Set aux frequency
                            }

                            if(bitfield.interruptMask != oldBitfield.interruptMask) {
                                // Enable/disable the AI interrupt
                            }

                            if(bitfield.sampleCounterReset) {
                                bitfield.sampleCounterReset = 0;

                                // Reset AI sample counter to 0
                                __AIRegs[2] = 0;
                            }
                            
                            newVal = bitfield.raw;
                        } break;
                        // AI Volume Register
                        case 1:
                        {
                            VolumeRegister bitfield;
                            bitfield.raw = newVal;
                            VolumeRegister oldBitfield;
                            oldBitfield.raw = oldVal;

                            if(bitfield.left != oldBitfield.left) {
                                // Set left channel volume
                            }

                            if(bitfield.right != oldBitfield.right) {
                                // Set right channel volume
                            }
                        } break;

                        // AI Sample Counter
                        case 2:
                        {

                        } break;
                        // AI Interrupt Timing
                        case 3:
                        {

                        } break;
                        default:
                            // Unhandled register
                            break;
                    }

                    if(reg <= 3) {
                        __AIRegs[reg] = newVal;
                    }
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