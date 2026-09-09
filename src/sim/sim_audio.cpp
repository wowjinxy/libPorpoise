#include <dolphin/types.h>
#include <dolphin/hw_regs.h>

#include "simulator/sim_audio.hpp"
#include "simulator/sim_ai.hpp"
#include "simulator/sim_MessageQueue.hpp"
#include "simulator/sim_dsp.hpp"

#include <SDL2/SDL.h>
#include <string>
#include <format>

#include <stdlib.h>

namespace SIM::Audio {
static SIM::MessageQueue sMessageQueue = SIM::MessageQueue<SIM::Audio::ThreadMessage>(256);
static SDL_AudioSpec sAudioSpec;
static int sSampleRate = 0;

static constexpr auto OutputSampleRate48 = 48000;
static constexpr auto OutputSampleRate32 = 32000;
static constexpr auto OutputBytesPerSample = 2;
static constexpr auto OutputNumChannels = 2;
static constexpr auto SdlBufferSamples = 300;

static void StopAudio() {
    SDL_CloseAudio();
    // do we need to wait for this at all?
}

static void StartAudio() {
    if(sSampleRate == 0) {
        sAudioSpec.freq = OutputSampleRate48;
    } else {
        sAudioSpec.freq = OutputSampleRate32;
    }
    
    sAudioSpec.format = AUDIO_S16LSB;
    sAudioSpec.channels = 2;

    sAudioSpec.samples = SdlBufferSamples;
    sAudioSpec.callback = SDLCallback;
    if(SDL_OpenAudio(&sAudioSpec, NULL) < 0) {
        std::string errorString = std::format("Error opening audio: {}", SDL_GetError());
        SDL_ShowSimpleMessageBox(0, "Audio Error", errorString.c_str(), nullptr);
    }
    SDL_PauseAudio(0);
}


void Init() {
    StartAudio();
}

void SetSampleRate(int aiSampleRate) {
    if(aiSampleRate != sSampleRate) {
        StopAudio();
        sSampleRate = aiSampleRate;
        StartAudio();
    }
}

void SDLCallback(void *userdata, u8 *stream, int len) {
    int samples = len / (OutputBytesPerSample);
    int frames = samples / OutputNumChannels;

    SIM::AI::StereoFrame * outputFrames = (SIM::AI::StereoFrame *)stream;


    u32 num = SIM::AI::ConsumeAudio(frames, outputFrames);
}

}
