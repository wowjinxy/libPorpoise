#include <dolphin/types.h>
#include <dolphin/hw_regs.h>

#include "simulator/sim_audio.hpp"
#include "simulator/sim_ai.hpp"
#include "simulator/sim_MessageQueue.hpp"

#include <SDL2/SDL.h>
#include <string>
#include <format>

namespace SIM::Audio {
static SIM::MessageQueue sMessageQueue = SIM::MessageQueue<SIM::Audio::ThreadMessage>(256);
static SDL_AudioSpec sAudioSpec;

static constexpr auto OutputSampleRrate = 48000;
static constexpr auto OutputBytesPerSample = 2;
static constexpr auto OutputNumChannels = 2;


void Init() {
    sAudioSpec.freq = OutputSampleRrate;
    sAudioSpec.format = AUDIO_S16LSB;
    sAudioSpec.channels = 2;

    sAudioSpec.samples = 2000;
    sAudioSpec.callback = SDLCallback;
    if(SDL_OpenAudio(&sAudioSpec, NULL) < 0) {
        std::string errorString = std::format("Error opening audio: {}", SDL_GetError());
        SDL_ShowSimpleMessageBox(0, "Audio Error", errorString.c_str(), nullptr);
    }
    SDL_PauseAudio(0);
}

void SDLCallback(void *userdata, u8 *stream, int len) {
    int samples = len / (OutputBytesPerSample * OutputNumChannels);


    
    // Get AI control register
    SIM::AI::ControlRegister aiCtrl;
    aiCtrl.raw = __AIRegs[0];

    if(aiCtrl.playingStatus) {
        __AIRegs[2] += samples;
    }
}

}
