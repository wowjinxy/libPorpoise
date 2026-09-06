#ifndef LIBPORPOISE_SIM_AUDIO_HPP
#define LIBPORPOISE_SIM_AUDIO_HPP

#include <dolphin/types.h>

namespace SIM::Audio {

enum class ThreadMessageType {
 SetRegValue,
 Count
};

struct ThreadMessage {
 ThreadMessage(){};
 ThreadMessageType mType;
};

void Init();
int MainThread(void * arg);
void SDLCallback(void *userdata, u8 *stream, int len);

}

#endif
