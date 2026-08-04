#ifndef LIBPORPOISE_SIM_GX_THREAD_HPP
#define LIBPORPOISE_SIM_GX_THREAD_HPP

#include <dolphin/types.h>
#include <SDL2/SDL_mutex.h>


namespace SIM::GX {

enum class ThreadMessageType {
 Fifo,
 TakeRenderContext,
 Count
};

struct ThreadMessage {
 ThreadMessageType mType;
 u8 mData[8];
 size_t mDataLen;
};

void Init();
int MainThread(void * arg);
template <typename T>
void SendFifoMessage(T data);
bool IsThreadDone();
void TakeRenderContext();
void GiveRenderContext();

}

#endif
