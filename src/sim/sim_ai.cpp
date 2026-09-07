#include <dolphin/types.h>
#include <dolphin/hw_regs.h>
#include <dolphin/os/OSInterrupt.h>

#include "simulator/sim_ai.hpp"
#include "simulator/sim_MessageQueue.hpp"
#include "simulator/sim_memory.hpp"

#include <SDL2/SDL.h>

namespace SIM::AI {
static SDL_Thread* sAiThread;
static SIM::MessageQueue sMessageQueue = SIM::MessageQueue<SIM::AI::ThreadMessage>(256);
static RingBuffer sAudioBuffer{48000};

static StereoFrame * sDmaStartAddress = nullptr;
static u32 sDmaFullLength = 0;
static StereoFrame * sDmaAddress = nullptr;
static s32 sDmaLength = 0; /* Remaining DMA length in stereo frames */
static bool sDmaStarted = false;
static SDL_mutex * sDmaMutex;

static void ProcessDma() {
    u32 framesCopied = 0;
    SDL_LockMutex(sDmaMutex);
    if(sDmaLength > 0) {
        framesCopied = sAudioBuffer.Write(sDmaAddress, sDmaLength);
        sDmaAddress += framesCopied;
        sDmaLength = sDmaLength - framesCopied;
        if(sDmaLength <= 0) {
            SDL_UnlockMutex(sDmaMutex);
            CallDmaInterrupt();
            SDL_LockMutex(sDmaMutex);
            sDmaAddress = sDmaStartAddress;
            sDmaLength = sDmaFullLength;
        }
    }
    SDL_UnlockMutex(sDmaMutex);
}

void Init() {
    sDmaMutex = SDL_CreateMutex();
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
            case ThreadMessageType::InitDma:
                {
                    SDL_LockMutex(sDmaMutex);
                    sDmaStartAddress = msg.mInitDma.startAddr;
                    sDmaFullLength = msg.mInitDma.length / 4;
                    SDL_UnlockMutex(sDmaMutex);
                } break;
            case ThreadMessageType::StartDma:
                {
                    SDL_LockMutex(sDmaMutex);
                    sDmaAddress = sDmaStartAddress;
                    sDmaLength = sDmaFullLength;
                    sDmaStarted = true;
                    SDL_UnlockMutex(sDmaMutex);
                } break;
            case ThreadMessageType::StopDma:
                {
                    SDL_LockMutex(sDmaMutex);
                    sDmaStarted = false;
                    SDL_UnlockMutex(sDmaMutex);
                }
            default:
                break;
        }
    }
    return 0;
}

void CallDmaInterrupt() {
    __OSInterruptHandler aiHandler = __OSGetInterruptHandler(__OS_INTERRUPT_DSP_AI);
    if(aiHandler) {
        aiHandler(__OS_INTERRUPT_DSP_AI, nullptr);
    }
}

void CallAiInterrupt() {
    __OSInterruptHandler aiHandler = __OSGetInterruptHandler(__OS_INTERRUPT_AI_AI);
    if(aiHandler) {
        aiHandler(__OS_INTERRUPT_AI_AI, nullptr);
    }
}

u32 ConsumeAudio(u32 numFrames, StereoFrame * outputBuffer) {
    if((__AIRegs[2] < __AIRegs[3]) && ((__AIRegs[2] + numFrames) >= __AIRegs[3])) {
        CallAiInterrupt();
    }
    SIM::AI::ControlRegister aiCtrl;
    aiCtrl.raw = __AIRegs[0];

    if(aiCtrl.playingStatus) {
        __AIRegs[2] += numFrames;
    }

    if(sDmaStarted && sAudioBuffer.AvailableToRead() < numFrames) {
        ProcessDma();
    }
    u32 retVal = sAudioBuffer.Read(outputBuffer, numFrames);

    return retVal;
}

void InitDma(u32 startMemHndl, u32 length) {
    SIM::AI::ThreadMessage msg;
    msg.mType = ThreadMessageType::InitDma;
    msg.mInitDma.startAddr = (StereoFrame*)SIM::Memory::MemoryHandleToAddress(startMemHndl);
    msg.mInitDma.length = length;
    sMessageQueue.SendMessage(msg);
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

void SIM_AIInitDma(u32 startMemHndl, u32 length) {
    SIM::AI::InitDma(startMemHndl, length);
}

void SIM_AIStartDma() {
    SIM::AI::ThreadMessage msg;
    msg.mType = SIM::AI::ThreadMessageType::StartDma;
    SIM::AI::sMessageQueue.SendMessage(msg);
}