/**
 * @file PAD.c
 * @brief GameCube Controller API Implementation for libPorpoise
 * 
 * PC implementation using SDL2 for gamepad input with keyboard fallback.
 * 
 * Copyright (c) 2025 libPorpoise Contributors
 * SPDX-License-Identifier: MIT
 */

#include <dolphin/pad.h>
#include <dolphin/os.h>
#include <string.h>
#include <SDL2/SDL.h>

/*---------------------------------------------------------------------------*
    Internal State
 *---------------------------------------------------------------------------*/

static BOOL s_initialized = FALSE;
static BOOL s_sdl_initialized = FALSE;

// SDL gamepad handles (NULL if not connected)
static SDL_GameController* s_gamepads[PAD_MAX_CONTROLLERS] = {NULL};

// Origin/calibration data
static PADStatus s_origin[PAD_MAX_CONTROLLERS];

// Controller status flags
static u32 s_enabled_bits = 0;          // Which channels are enabled
static u32 s_resetting_bits = 0;        // Which channels are currently resetting
static u32 s_waiting_bits = 0;          // Which channels are waiting for connection

// Analog mode (controls which analog inputs are active)
static u32 s_analog_mode = PAD_MODE_3;

// Sampling callback
static PADSamplingCallback s_sampling_callback = NULL;

// Keyboard fallback state (for channel 0 only)
static struct {
    BOOL enabled;
    const Uint8* keys;
} s_keyboard;

/*---------------------------------------------------------------------------*
    Keyboard Mapping (fallback for channel 0)
 *---------------------------------------------------------------------------*/

#define KEY_UP      SDL_SCANCODE_UP
#define KEY_DOWN    SDL_SCANCODE_DOWN
#define KEY_LEFT    SDL_SCANCODE_LEFT
#define KEY_RIGHT   SDL_SCANCODE_RIGHT
#define KEY_A       SDL_SCANCODE_Z
#define KEY_B       SDL_SCANCODE_X
#define KEY_X       SDL_SCANCODE_C
#define KEY_Y       SDL_SCANCODE_V
#define KEY_START   SDL_SCANCODE_RETURN
#define KEY_L       SDL_SCANCODE_A
#define KEY_R       SDL_SCANCODE_S
#define KEY_Z       SDL_SCANCODE_D

// C-stick
#define KEY_CUP     SDL_SCANCODE_I
#define KEY_CDOWN   SDL_SCANCODE_K
#define KEY_CLEFT   SDL_SCANCODE_J
#define KEY_CRIGHT  SDL_SCANCODE_L

/*---------------------------------------------------------------------------*
    Internal Functions
 *---------------------------------------------------------------------------*/

/**
 * @brief Initialize SDL2 gamepad subsystem
 */
static BOOL InitSDL(void) {
    if (s_sdl_initialized) {
        return TRUE;
    }
    
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) < 0) {
        OSReport("PAD: Failed to initialize SDL gamepad: %s\n", SDL_GetError());
        
        // Try keyboard fallback
        if (SDL_InitSubSystem(SDL_INIT_EVENTS) < 0) {
            OSReport("PAD: Failed to initialize SDL events: %s\n", SDL_GetError());
            return FALSE;
        }
        
        OSReport("PAD: Using keyboard fallback for player 1\n");
        s_keyboard.enabled = TRUE;
        s_keyboard.keys = SDL_GetKeyboardState(NULL);
    }
    
    s_sdl_initialized = TRUE;
    return TRUE;
}

/**
 * @brief Open a gamepad for the specified channel
 */
static void OpenGamepad(s32 chan) {
    if (chan < 0 || chan >= PAD_MAX_CONTROLLERS) {
        return;
    }
    
    // Close existing gamepad
    if (s_gamepads[chan]) {
        SDL_GameControllerClose(s_gamepads[chan]);
        s_gamepads[chan] = NULL;
    }
    
    // Try to open gamepad
    s_gamepads[chan] = SDL_GameControllerOpen(chan);
    if (s_gamepads[chan]) {
        OSReport("PAD: Channel %d connected - %s\n", chan, 
                 SDL_GameControllerName(s_gamepads[chan]));
    }
}

/**
 * @brief Scan for connected gamepads
 */
static void ScanGamepads(void) {
    for (s32 chan = 0; chan < PAD_MAX_CONTROLLERS; chan++) {
        u32 chanBit = PAD_CHAN0_BIT >> chan;
        
        // Skip if not enabled
        if (!(s_enabled_bits & chanBit)) {
            continue;
        }
        
        // Check if we need to open this gamepad
        if (!s_gamepads[chan]) {
            int numJoysticks = SDL_NumJoysticks();
            if (chan < numJoysticks) {
                if (SDL_IsGameController(chan)) {
                    OpenGamepad(chan);
                }
            }
        }
    }
}

/**
 * @brief Update origin/calibration data
 */
static void UpdateOrigin(s32 chan) {
    PADStatus* origin = &s_origin[chan];
    
    // Adjust for analog mode
    switch (s_analog_mode & 7) {
        case PAD_MODE_0:
        case PAD_MODE_5:
        case PAD_MODE_6:
        case PAD_MODE_7:
            origin->triggerLeft  &= ~15;
            origin->triggerRight &= ~15;
            origin->analogA      &= ~15;
            origin->analogB      &= ~15;
            break;
        case PAD_MODE_1:
            origin->substickX    &= ~15;
            origin->substickY    &= ~15;
            origin->analogA      &= ~15;
            origin->analogB      &= ~15;
            break;
        case PAD_MODE_2:
            origin->substickX    &= ~15;
            origin->substickY    &= ~15;
            origin->triggerLeft  &= ~15;
            origin->triggerRight &= ~15;
            break;
        case PAD_MODE_3:
        case PAD_MODE_4:
            break;
    }
}

/**
 * @brief Read keyboard input (fallback for channel 0)
 */
static void ReadKeyboard(PADStatus* status) {
    if (!s_keyboard.enabled || !s_keyboard.keys) {
        status->err = PAD_ERR_NO_CONTROLLER;
        memset(status, 0, offsetof(PADStatus, err));
        return;
    }
    
    // Update keyboard state
    SDL_PumpEvents();
    s_keyboard.keys = SDL_GetKeyboardState(NULL);
    
    // Clear status
    memset(status, 0, sizeof(PADStatus));
    status->err = PAD_ERR_NONE;
    
    // D-pad
    if (s_keyboard.keys[KEY_LEFT])  status->button |= PAD_BUTTON_LEFT;
    if (s_keyboard.keys[KEY_RIGHT]) status->button |= PAD_BUTTON_RIGHT;
    if (s_keyboard.keys[KEY_UP])    status->button |= PAD_BUTTON_UP;
    if (s_keyboard.keys[KEY_DOWN])  status->button |= PAD_BUTTON_DOWN;
    
    // Buttons
    if (s_keyboard.keys[KEY_A])     status->button |= PAD_BUTTON_A;
    if (s_keyboard.keys[KEY_B])     status->button |= PAD_BUTTON_B;
    if (s_keyboard.keys[KEY_X])     status->button |= PAD_BUTTON_X;
    if (s_keyboard.keys[KEY_Y])     status->button |= PAD_BUTTON_Y;
    if (s_keyboard.keys[KEY_START]) status->button |= PAD_BUTTON_START;
    if (s_keyboard.keys[KEY_Z])     status->button |= PAD_TRIGGER_Z;
    if (s_keyboard.keys[KEY_L])     status->button |= PAD_TRIGGER_L;
    if (s_keyboard.keys[KEY_R])     status->button |= PAD_TRIGGER_R;
    
    // Analog stick (digital simulation: -128 or 127)
    if (s_keyboard.keys[KEY_LEFT])  status->stickX = -100;
    if (s_keyboard.keys[KEY_RIGHT]) status->stickX = 100;
    if (s_keyboard.keys[KEY_UP])    status->stickY = 100;
    if (s_keyboard.keys[KEY_DOWN])  status->stickY = -100;
    
    // C-stick
    if (s_keyboard.keys[KEY_CLEFT])  status->substickX = -100;
    if (s_keyboard.keys[KEY_CRIGHT]) status->substickX = 100;
    if (s_keyboard.keys[KEY_CUP])    status->substickY = 100;
    if (s_keyboard.keys[KEY_CDOWN])  status->substickY = -100;
    
    // Triggers (digital: 0 or 255)
    if (s_keyboard.keys[KEY_L]) status->triggerLeft = 255;
    if (s_keyboard.keys[KEY_R]) status->triggerRight = 255;
}

/**
 * @brief Read gamepad input via SDL2
 */
static void ReadGamepad(s32 chan, PADStatus* status) {
    SDL_GameController* pad = s_gamepads[chan];
    
    if (!pad) {
        status->err = PAD_ERR_NO_CONTROLLER;
        memset(status, 0, offsetof(PADStatus, err));
        return;
    }
    
    // Clear status
    memset(status, 0, sizeof(PADStatus));
    status->err = PAD_ERR_NONE;
    
    // Buttons
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
        status->button |= PAD_BUTTON_LEFT;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
        status->button |= PAD_BUTTON_RIGHT;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP))
        status->button |= PAD_BUTTON_UP;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
        status->button |= PAD_BUTTON_DOWN;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A))
        status->button |= PAD_BUTTON_A;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B))
        status->button |= PAD_BUTTON_B;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X))
        status->button |= PAD_BUTTON_X;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y))
        status->button |= PAD_BUTTON_Y;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START))
        status->button |= PAD_BUTTON_START;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
        status->button |= PAD_TRIGGER_R;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
        status->button |= PAD_TRIGGER_L;
    
    // Check for Z button (map to right stick press or back button)
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSTICK) ||
        SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK))
        status->button |= PAD_TRIGGER_Z;
    
    // Analog stick (SDL gives -32768 to 32767, convert to -128 to 127)
    s16 axisX = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
    s16 axisY = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
    status->stickX = (s8)(axisX / 256);
    status->stickY = (s8)(-axisY / 256);  // Invert Y axis
    
    // C-stick (right analog)
    s16 cAxisX = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
    s16 cAxisY = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY);
    status->substickX = (s8)(cAxisX / 256);
    status->substickY = (s8)(-cAxisY / 256);  // Invert Y axis
    
    // Triggers (SDL gives 0-32767, convert to 0-255)
    u16 trigL = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    u16 trigR = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    status->triggerLeft = (u8)(trigL / 128);
    status->triggerRight = (u8)(trigR / 128);
    
    // Analog A/B not supported on most modern controllers
    status->analogA = 0;
    status->analogB = 0;
    
    // Apply origin calibration
    PADStatus* origin = &s_origin[chan];
    status->stickX -= origin->stickX;
    status->stickY -= origin->stickY;
    status->substickX -= origin->substickX;
    status->substickY -= origin->substickY;
    
    if (status->triggerLeft > origin->triggerLeft) {
        status->triggerLeft -= origin->triggerLeft;
    } else {
        status->triggerLeft = 0;
    }
    
    if (status->triggerRight > origin->triggerRight) {
        status->triggerRight -= origin->triggerRight;
    } else {
        status->triggerRight = 0;
    }
}

/*---------------------------------------------------------------------------*
    Public API
 *---------------------------------------------------------------------------*/

BOOL PADInit(void) {
    if (s_initialized) {
        return TRUE;
    }
    
    OSReport("PAD: Initializing controller subsystem...\n");
    
    // Initialize SDL
    if (!InitSDL()) {
        OSReport("PAD: Failed to initialize SDL\n");
        return FALSE;
    }
    
    // Clear state
    memset(s_gamepads, 0, sizeof(s_gamepads));
    memset(s_origin, 0, sizeof(s_origin));
    s_enabled_bits = 0;
    s_resetting_bits = 0;
    s_waiting_bits = 0;
    s_analog_mode = PAD_MODE_3;
    s_sampling_callback = NULL;
    
    s_initialized = TRUE;
    
    // Reset all channels
    return PADReset(PAD_CHAN0_BIT | PAD_CHAN1_BIT | PAD_CHAN2_BIT | PAD_CHAN3_BIT);
}

BOOL PADReset(u32 mask) {
    if (!s_initialized) {
        return FALSE;
    }
    
    BOOL enabled = OSDisableInterrupts();
    
    for (s32 chan = 0; chan < PAD_MAX_CONTROLLERS; chan++) {
        u32 chanBit = PAD_CHAN0_BIT >> chan;
        
        if (mask & chanBit) {
            // Enable this channel
            s_enabled_bits |= chanBit;
            s_resetting_bits |= chanBit;
            
            // Clear origin
            memset(&s_origin[chan], 0, sizeof(PADStatus));
            
            // Try to open gamepad
            if (!s_keyboard.enabled || chan != 0) {
                OpenGamepad(chan);
            }
            
            s_resetting_bits &= ~chanBit;
        }
    }
    
    OSRestoreInterrupts(enabled);
    return TRUE;
}

BOOL PADRecalibrate(u32 mask) {
    if (!s_initialized) {
        return FALSE;
    }
    
    // For PC implementation, recalibration just resets the origin
    // We could capture current stick/trigger positions as new baseline
    BOOL enabled = OSDisableInterrupts();
    
    for (s32 chan = 0; chan < PAD_MAX_CONTROLLERS; chan++) {
        u32 chanBit = PAD_CHAN0_BIT >> chan;
        
        if (mask & chanBit) {
            memset(&s_origin[chan], 0, sizeof(PADStatus));
            UpdateOrigin(chan);
        }
    }
    
    OSRestoreInterrupts(enabled);
    return TRUE;
}

u32 PADRead(PADStatus* status) {
    if (!s_initialized || !status) {
        if (status) {
            for (s32 i = 0; i < PAD_MAX_CONTROLLERS; i++) {
                status[i].err = PAD_ERR_NOT_READY;
                memset(&status[i], 0, offsetof(PADStatus, err));
            }
        }
        return 0;
    }
    
    // Pump SDL events
    if (s_sdl_initialized) {
        SDL_PumpEvents();
    }
    
    // Scan for new gamepads
    ScanGamepads();
    
    // Call sampling callback
    if (s_sampling_callback) {
        s_sampling_callback();
    }
    
    u32 motor = 0;
    
    for (s32 chan = 0; chan < PAD_MAX_CONTROLLERS; chan++) {
        PADStatus* st = &status[chan];
        u32 chanBit = PAD_CHAN0_BIT >> chan;
        
        // Check if channel is enabled
        if (!(s_enabled_bits & chanBit)) {
            st->err = PAD_ERR_NO_CONTROLLER;
            memset(st, 0, offsetof(PADStatus, err));
            continue;
        }
        
        // Check if resetting
        if (s_resetting_bits & chanBit) {
            st->err = PAD_ERR_NOT_READY;
            memset(st, 0, offsetof(PADStatus, err));
            continue;
        }
        
        // Read input (keyboard for chan 0 if no gamepad, else gamepad)
        if (chan == 0 && s_keyboard.enabled && !s_gamepads[0]) {
            ReadKeyboard(st);
        } else {
            ReadGamepad(chan, st);
        }
        
        // Check if controller supports rumble
        if (s_gamepads[chan]) {
            SDL_Joystick* joy = SDL_GameControllerGetJoystick(s_gamepads[chan]);
            if (joy && SDL_JoystickIsHaptic(joy)) {
                motor |= chanBit;
            }
        }
    }
    
    return motor;
}

void PADControlMotor(s32 chan, u32 command) {
    if (!s_initialized || chan < 0 || chan >= PAD_MAX_CONTROLLERS) {
        return;
    }
    
    SDL_GameController* pad = s_gamepads[chan];
    if (!pad) {
        return;
    }
    
    SDL_Joystick* joy = SDL_GameControllerGetJoystick(pad);
    if (!joy || !SDL_JoystickIsHaptic(joy)) {
        return;
    }
    
    SDL_Haptic* haptic = SDL_HapticOpenFromJoystick(joy);
    if (!haptic) {
        return;
    }
    
    if (command == PAD_MOTOR_RUMBLE) {
        // Start rumble
        if (SDL_HapticRumbleSupported(haptic)) {
            SDL_HapticRumbleInit(haptic);
            SDL_HapticRumblePlay(haptic, 0.5f, 1000);  // 50% strength, 1 second
        }
    } else {
        // Stop rumble
        SDL_HapticRumbleStop(haptic);
    }
    
    SDL_HapticClose(haptic);
}

void PADControlAllMotors(const u32* commandArray) {
    if (!s_initialized || !commandArray) {
        return;
    }
    
    for (s32 chan = 0; chan < PAD_MAX_CONTROLLERS; chan++) {
        PADControlMotor(chan, commandArray[chan]);
    }
}

BOOL PADGetType(s32 chan, u32* type) {
    if (!s_initialized || chan < 0 || chan >= PAD_MAX_CONTROLLERS || !type) {
        return FALSE;
    }
    
    u32 chanBit = PAD_CHAN0_BIT >> chan;
    
    // Check if enabled and ready
    if (!(s_enabled_bits & chanBit) || (s_resetting_bits & chanBit)) {
        return FALSE;
    }
    
    // For PC implementation, return a generic controller type
    *type = 0x09000000;  // Standard controller
    return TRUE;
}

BOOL PADSync(void) {
    return (s_resetting_bits == 0);
}

void PADSetAnalogMode(u32 mode) {
    if (mode >= 8) {
        return;
    }
    
    BOOL enabled = OSDisableInterrupts();
    s_analog_mode = mode;
    OSRestoreInterrupts(enabled);
}

void PADSetSamplingRate(u32 msec) {
    (void)msec;
    // No-op on PC - sampling rate tied to PADRead() calls
}

PADSamplingCallback PADSetSamplingCallback(PADSamplingCallback callback) {
    PADSamplingCallback prev = s_sampling_callback;
    s_sampling_callback = callback;
    return prev;
}

