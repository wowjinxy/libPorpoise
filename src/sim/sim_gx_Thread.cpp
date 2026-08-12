#include <cstring>

#include "simulator/sim.hpp"
#include "simulator/sim_gx_Thread.hpp"
#include "simulator/sim_gx_Thread.h"
#include "simulator/sim_gx_CommandProcessor.hpp"
#include "simulator/sim_gx_State.hpp"
#include "simulator/sim_gx_TextureManager.hpp"
#include "simulator/sim_MessageQueue.hpp"

#include <SDL2/SDL.h>

#define COMMAND_PROCESSOR_DEBUG 0

static SIM::GX::CommandProcessor sCommandProcessor = SIM::GX::CommandProcessor();
static SIM::MessageQueue sMessageQueue = SIM::MessageQueue<SIM::GX::ThreadMessage>(256 * 1024);
static SDL_Thread * sGxMainThread;
static SDL_sem* sGxRenderContextSemaphore;


namespace SIM::GX {
void Init() {
    InitGlobalState();

    sGxMainThread = SDL_CreateThread(MainThread, "GXMainThread", nullptr);
}

int MainThread(void * arg) {

    SIM::AcquireRenderContext();
    sGxRenderContextSemaphore = SDL_CreateSemaphore(0);
    while(true) {
        //Wait for messages
        auto msg = sMessageQueue.ReceiveMessage();

        switch(msg.mType) {
            case ThreadMessageType::Fifo:
                sCommandProcessor.ProcessFifoData(msg.mData, msg.mDataLen);
                break;
            case ThreadMessageType::SetVertexArray:
                {
                    u8 * msgData = msg.mData;
                    SIM::GX::VertexArray* vtxArray = (SIM::GX::VertexArray*)msgData;
                    SIM::GX::GetGlobalState().SetVertexArray(vtxArray->attribute, *vtxArray);
                } break;
            case ThreadMessageType::InitTexObj:
                {
                    auto& manager = SIM::GX::TextureManager::GetInstance();

                    manager.InitTexObj(msg.mInitTexObj.obj, 
                                       msg.mInitTexObj.imagePtr, 
                                       msg.mInitTexObj.width, 
                                       msg.mInitTexObj.height, 
                                       msg.mInitTexObj.format, 
                                       msg.mInitTexObj.wrapS, 
                                       msg.mInitTexObj.wrapT, 
                                       msg.mInitTexObj.mipmap);
                } break;
            case ThreadMessageType::LoadTexObj:
                {
                    auto& manager = SIM::GX::TextureManager::GetInstance();

                    manager.LoadTexObj(msg.mLoadTexObj.obj, msg.mLoadTexObj.map);
                } break;
            case ThreadMessageType::TakeRenderContext:
                {
                    //Release render context
                    SIM::ReleaseRenderContext();

                    //Call the callback in the message
                    u64 * dataPtr = (u64*)msg.mData;
                    SDL_sem* semaphore = (SDL_sem*)(*dataPtr);
                    if(semaphore) {
                        //Notify sending thread
                        SDL_SemPost(semaphore);
                    }

                    // Pause until the context is given back
                    SDL_SemWait(sGxRenderContextSemaphore);

                    //Reacquire context
                    SIM::AcquireRenderContext();
                }
                break;
            default:
                break;
        }
    }

    return 0;
}

template <typename T>
void SendFifoMessage(T data) {
    ThreadMessage msg;
    msg.mType = ThreadMessageType::Fifo;
    size_t dataLen = sizeof(T);
    if(dataLen > 8) {
        //Message too big! (todo maybe increase it)
        return;
    }

    std::memcpy(msg.mData, &data, dataLen);
    msg.mDataLen = dataLen;
    #if COMMAND_PROCESSOR_DEBUG
    sCommandProcessor.ProcessFifoData(msg.mData, msg.mDataLen);
    #else
    sMessageQueue.SendMessage(msg);
    #endif
}

void SendThreadMessage(ThreadMessage& msg) {
    sMessageQueue.SendMessage(msg);
}

//Take the render context from the GX Thread
//Pause the GX thread until the render context is given back
void TakeRenderContext() {
    SDL_sem* semaphore = SDL_CreateSemaphore(0);
    ThreadMessage msg;
    msg.mType = ThreadMessageType::TakeRenderContext;
    std::memcpy(msg.mData, &semaphore, sizeof(void*));

    sMessageQueue.SendMessage(msg);

    SDL_SemWait(semaphore);
    SDL_DestroySemaphore(semaphore);
}

// Give the render context back to the GX thread and resume it
void GiveRenderContext() {
    SDL_SemPost(sGxRenderContextSemaphore);
}

bool IsThreadDone() {
    return sMessageQueue.empty();
}

}

// C APIs for GX Thread/Fifo

void SIM_GX_Fifo_SendU8(u8 data) {
    SIM::GX::SendFifoMessage<u8>(data);
}

void SIM_GX_Fifo_SendU16(u16 data) {
    SIM::GX::SendFifoMessage<u16>(data);
}

void SIM_GX_Fifo_SendS16(s16 data) {
    SIM::GX::SendFifoMessage<s16>(data);
}

void SIM_GX_Fifo_SendU32(u32 data) {
    SIM::GX::SendFifoMessage<u32>(data);
}

void SIM_GX_Fifo_SendF32(f32 data) {
    SIM::GX::SendFifoMessage<f32>(data);
}

void SIM_GX_Fifo_SendU64(u64 data) {
    SIM::GX::SendFifoMessage<u64>(data);
}

void SIM_GX_CommandProcessor_SetVertexArray(GXAttr attr, void * ptr, int stride) {
    SIM::GX::ThreadMessage msg;
    msg.mType = SIM::GX::ThreadMessageType::SetVertexArray;
    msg.mDataLen = sizeof(SIM::GX::VertexArray);
    msg.mVertexArray.attribute = attr;
    msg.mVertexArray.mArrayPtr = ptr;
    msg.mVertexArray.mStride = stride;
    sMessageQueue.SendMessage(msg);
}