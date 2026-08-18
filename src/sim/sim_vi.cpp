#include <SDL2/SDL.h>

#include <dolphin/types.h>

#include "simulator/sim_vi.h"
#include "dolphin/vi/vitypes.h"

namespace SIM::VI {

static u32 s_waitForRetraceCount = 0;
static u32 s_retraceCount = 0;
static SDL_cond * s_retraceCond;

static VIRetraceCallback s_preRetraceCallback = nullptr;
static VIRetraceCallback s_postRetraceCallback = nullptr;


void Init() {
    s_retraceCond = SDL_CreateCond();
}

void HandlePreRetrace() {
    if(s_preRetraceCallback) {
        s_preRetraceCallback(s_retraceCount);
    }
}

void HandlePostRetrace() {
    s_retraceCount++;
    SDL_CondBroadcast(s_retraceCond);
    s_waitForRetraceCount = 0;
    if(s_postRetraceCallback) {
        s_postRetraceCallback(s_retraceCount);
    }
}

void WaitForRetrace() {
    s_waitForRetraceCount++;
    SDL_mutex * dummy = SDL_CreateMutex();
    SDL_LockMutex(dummy);
    int result = SDL_CondWait(s_retraceCond, dummy);
    SDL_DestroyMutex(dummy);
}

u32 GetWaitForRetraceCount() {
    return s_waitForRetraceCount;
}

u32 GetRetraceCount() {
    return s_retraceCount;
}

void SetPreRetraceCallback(VIRetraceCallback callback) {
    s_preRetraceCallback = callback;
}

void SetPostRetraceCallback(VIRetraceCallback callback) {
    s_postRetraceCallback = callback;
}
}

// C APIs
void SIM_VIWaitForRetrace() {
    SIM::VI::WaitForRetrace();
}

void SIM_VISetPreRetraceCallback(VIRetraceCallback callback) {
    SIM::VI::SetPreRetraceCallback(callback);
}

void SIM_VISetPostRetraceCallback(VIRetraceCallback callback) {
    SIM::VI::SetPostRetraceCallback(callback);
}

u32 SIM_VIGetRetraceCount() {
    return SIM::VI::GetRetraceCount();
}