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

static constexpr auto OutputSampleRrate = 48000;
static constexpr auto OutputBytesPerSample = 2;
static constexpr auto OutputNumChannels = 2;


void Init() {
    sAudioSpec.freq = OutputSampleRrate;
    sAudioSpec.format = AUDIO_S16LSB;
    sAudioSpec.channels = 2;

    sAudioSpec.samples = 300;
    sAudioSpec.callback = SDLCallback;
    if(SDL_OpenAudio(&sAudioSpec, NULL) < 0) {
        std::string errorString = std::format("Error opening audio: {}", SDL_GetError());
        SDL_ShowSimpleMessageBox(0, "Audio Error", errorString.c_str(), nullptr);
    }
    SDL_PauseAudio(0);
}

void SDLCallback(void *userdata, u8 *stream, int len) {
    int samples = len / (OutputBytesPerSample);
    int frames = samples / OutputNumChannels;

    SIM::AI::StereoFrame * outputFrames = (SIM::AI::StereoFrame *)stream;


    u32 num = SIM::AI::ConsumeAudio(frames, outputFrames);

    if(num > 0) {
    // This plays static, for testing
    //s16 * frames16 = (s16*)stream;
    //for(int i=0; i < len / 2; i++) {
    //    // for now fill the buffer up with random junk so we can hear "something"
    //    s16 value = rand();
    //    frames16[i] = value;
    //}
    }


}

}
