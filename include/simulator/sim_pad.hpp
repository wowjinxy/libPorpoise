#ifndef LIBPORPOISE_SIM_PAD_HPP
#define LIBPORPOISE_SIM_PAD_HPP

#include <SDL2/SDL_keyboard.h>
#include <dolphin/types.h>
#include <dolphin/pad.h>
#include "simulator/sim_pad.h"


namespace SIM::PAD {

void Read(PADStatus * status);

void HandleKey(SDL_Keycode key, bool isDown);

}


#endif
