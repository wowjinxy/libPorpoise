#ifndef LIBPORPOISE_SIM_DSP_HPP
#define LIBPORPOISE_SIM_DSP_HPP

#include <dolphin/types.h>
#include <SDL2/SDL_mutex.h>

#include "simulator/sim_dsp.h"

#include "simulator/sim_dsp_IMicrocode.hpp"

namespace SIM::DSP {

enum class ThreadMessageType {
 SendMailToDSP,
 ReadMailFromDSP,
 LoadMicrocode,
 Count
};

struct ReadMailMessage {
   SDL_sem * semaphore;
   u32 * mailValue;
};

struct ThreadMessage {
 ThreadMessage(){};
 ThreadMessageType mType;
 union {
   u32 mSendMail;
   ReadMailMessage mReadMail;
 };

};

void Init();
int MainThread(void * arg);
void SendMailToDSP(u32 mail);
u32 ReadMailFromDSP();
void CallInterrupt();
void UploadMicrocode(u32 iramMmemAddrHndl, u32 iramAddr, u32 iramLength, u32 aramMmemAddr, u32 dspInitVector);

}

#endif
