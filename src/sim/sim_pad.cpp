#include "simulator/sim_pad.hpp"

#include <SDL2/SDL_keyboard.h>
#include <dolphin.h>


static PADStatus sStatus[PAD_MAX_CONTROLLERS];

namespace SIM::PAD {

static void SetButtonBit(u16& button, u16 bit, bool isDown) {
    if(isDown) {
        button |= bit;
    } else {
        button &= ~(bit);
    }
}

void Read(PADStatus * status) {
    memcpy(status, sStatus, sizeof(PADStatus) * PAD_MAX_CONTROLLERS);
}

// Handles SDL Keyboard keys
void HandleKey(SDL_Keycode key, bool isDown) {
    auto& button = sStatus[0].button;

    switch(key) {
        case SDLK_z:
            SetButtonBit(button, PAD_BUTTON_A, isDown);
            break;
        case SDLK_x:
            SetButtonBit(button, PAD_BUTTON_B, isDown);
            break;
        case SDLK_c:
            SetButtonBit(button, PAD_BUTTON_X, isDown);
            break;
        case SDLK_v:
            SetButtonBit(button, PAD_BUTTON_Y, isDown);
            break;

        case SDLK_UP:
            SetButtonBit(button, PAD_BUTTON_UP, isDown);
            break;
        case SDLK_DOWN:
            SetButtonBit(button, PAD_BUTTON_UP, isDown);
            break;
        case SDLK_LEFT:
            SetButtonBit(button, PAD_BUTTON_UP, isDown);
            break;
        case SDLK_RIGHT:
            SetButtonBit(button, PAD_BUTTON_UP, isDown);
            break;

        default:
            break;
    }
}

}

// C APIs
void SIM_PAD_Read(PADStatus* status) {
    SIM::PAD::Read(status);
}