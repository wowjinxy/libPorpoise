/*---------------------------------------------------------------------------*
  PAD.c - GameCube Controller Input System
  
  ARCHITECTURAL DIFFERENCES: GC/Wii vs PC
  ========================================
  
  On GC/Wii (Serial Interface Hardware):
  ---------------------------------------
  - Controllers connect via Serial Interface (SI) hardware
  - SI handles automatic polling at configurable rate (6.75ms default)
  - Hardware DMA transfers controller data to memory
  - Supports 4 controller ports (SI channels 0-3)
  - Controllers send 8-byte packets with button/analog state
  - Wireless controllers require special probe/pairing commands
  - Origin calibration sent by controller at reset
  - Hardware handles trigger/stick analog data directly
  - Rumble motors controlled via SI command packets
  
  On PC (SDL2 Gamepad System):
  -----------------------------
  - Modern gamepads via SDL2 (Xbox, PlayStation, Switch Pro, etc.)
  - Manual polling via SDL_GameControllerGetButton/GetAxis
  - No hardware DMA - direct API calls
  - SDL2 handles device detection and hotplug
  - Analog values from SDL2 axes (-32768 to 32767)
  - No wireless pairing - OS handles Bluetooth
  - Origin calibration simulated in software
  - Keyboard fallback for Player 1 (no controller needed)
  - Rumble via SDL2 Haptic API
  
  WHY THE DIFFERENCE:
  - Original: Custom hardware interface, fixed packet format
  - PC: Industry-standard SDL2 library, flexible device support
  - Can't access serial interface hardware on PC
  - SDL2 provides better compatibility with modern controllers
  
  WHAT WE PRESERVE:
  - Same API surface (PADInit, PADRead, PADControlMotor, etc.)
  - PADStatus structure layout
  - Button bit definitions
  - Error codes (PAD_ERR_NONE, PAD_ERR_NO_CONTROLLER, etc.)
  - 4 controller support
  - Origin calibration concept
  - Analog modes
  
  WHAT'S DIFFERENT:
  - No SI hardware polling - we poll SDL2 in PADRead()
  - No hardware DMA - we fill PADStatus manually
  - Keyboard fallback option (original didn't have this)
  - SDL2 controller mapping (more flexible than SI)
  - Rumble intensity/duration controlled via SDL2 Haptic
  - No wireless pairing commands (OS handles it)
  
  KEYBOARD FALLBACK:
  - If no gamepads detected, Player 1 uses keyboard
  - Arrow keys: D-pad / Main stick
  - Z,X,C,V: A,B,X,Y buttons
  - A,S,D: L,R,Z triggers
  - I,K,J,L: C-stick
  - Enter: START button
 *---------------------------------------------------------------------------*/

#include <dolphin/pad.h>
#include <dolphin/os.h>
#include <string.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "PADConfig.h"

/*---------------------------------------------------------------------------*
    Internal State
 *---------------------------------------------------------------------------*/

// Initialization flags
static BOOL s_initialized = FALSE;       // PAD system initialized
static BOOL s_sdl_initialized = FALSE;   // SDL2 subsystem initialized

// SDL gamepad handles (NULL if not connected)
static SDL_GameController* s_gamepads[PAD_MAX_CONTROLLERS] = {NULL};

// Origin/calibration data (analog stick centers, trigger baselines)
static PADStatus s_origin[PAD_MAX_CONTROLLERS];

// Controller status flags
static u32 s_enabled_bits = 0;          // OR-ed PAD_CHANn_BIT of enabled channels
static u32 s_resetting_bits = 0;        // OR-ed PAD_CHANn_BIT of resetting channels
static u32 s_waiting_bits = 0;          // OR-ed PAD_CHANn_BIT waiting for connection

// Analog mode (controls which analog inputs are active)
// Mode 3 (default) = full stick + triggers, no analog A/B
static u32 s_analog_mode = PAD_MODE_3;

// Sampling callback (called during PADRead)
static PADSamplingCallback s_sampling_callback = NULL;

// Keyboard fallback state (for channel 0 only)
static struct {
    BOOL enabled;                        // TRUE if using keyboard
    const Uint8* keys;                   // Keyboard state array
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
  Name:         InitSDL

  Description:  Initialize SDL2 gamepad subsystem. If gamepad init fails,
                falls back to keyboard input for Player 1.
                
                On GC/Wii: SI hardware is initialized and polls automatically
                On PC: We initialize SDL2 for manual polling

  Arguments:    None

  Returns:      TRUE if SDL2 initialized (either gamepad or events)
                FALSE if complete failure
 *---------------------------------------------------------------------------*/
static BOOL InitSDL(void) {
    if (s_sdl_initialized) {
        return TRUE;
    }
    
    // Load configuration from pad_config.ini
    PADLoadConfig();
    
    // Try to initialize gamepad + haptic (rumble)
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

/*---------------------------------------------------------------------------*
  Name:         OpenGamepad

  Description:  Open a gamepad for the specified channel.
                Equivalent to detecting a controller on SI hardware.
                
                On GC/Wii: Controller detected via SI status register
                On PC: SDL_GameControllerOpen() opens indexed device

  Arguments:    chan    Channel number (0-3)

  Returns:      None (s_gamepads[chan] set to controller handle or NULL)
 *---------------------------------------------------------------------------*/
static void OpenGamepad(s32 chan) {
    if (chan < 0 || chan >= PAD_MAX_CONTROLLERS) {
        return;
    }
    
    // Close existing gamepad (if unplugged/replaced)
    if (s_gamepads[chan]) {
        SDL_GameControllerClose(s_gamepads[chan]);
        s_gamepads[chan] = NULL;
    }
    
    // Try to open gamepad at this index
    s_gamepads[chan] = SDL_GameControllerOpen(chan);
    if (s_gamepads[chan]) {
        OSReport("PAD: Channel %d connected - %s\n", chan, 
                 SDL_GameControllerName(s_gamepads[chan]));
    }
}

/*---------------------------------------------------------------------------*
  Name:         ScanGamepads

  Description:  Scan for connected gamepads and open any new ones.
                Equivalent to SI hardware detecting controller insertions.
                
                On GC/Wii: SI hardware automatically detects connections
                On PC: We manually check SDL joystick count

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
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

/*---------------------------------------------------------------------------*
  Name:         UpdateOrigin

  Description:  Update origin/calibration data for the specified channel.
                Adjusts origin based on analog mode (some modes disable
                certain analog inputs by masking them).
                
                On GC/Wii: Controller sends origin data in response to
                           CMD_READ_ORIGIN command
                On PC: We track origin in software for calibration

  Arguments:    chan    Channel number to update

  Returns:      None
 *---------------------------------------------------------------------------*/
static void UpdateOrigin(s32 chan) {
    PADStatus* origin = &s_origin[chan];
    
    // Adjust for analog mode - mask out unused analog inputs
    // This matches the original behavior where different modes
    // provide different precision for different inputs
    switch (s_analog_mode & 7) {
        case PAD_MODE_0:
        case PAD_MODE_5:
        case PAD_MODE_6:
        case PAD_MODE_7:
            // These modes reduce precision on triggers and analog buttons
            origin->triggerLeft  &= ~15;
            origin->triggerRight &= ~15;
            origin->analogA      &= ~15;
            origin->analogB      &= ~15;
            break;
        case PAD_MODE_1:
            // Reduced precision on C-stick and analog buttons
            origin->substickX    &= ~15;
            origin->substickY    &= ~15;
            origin->analogA      &= ~15;
            origin->analogB      &= ~15;
            break;
        case PAD_MODE_2:
            // Reduced precision on C-stick and triggers
            origin->substickX    &= ~15;
            origin->substickY    &= ~15;
            origin->triggerLeft  &= ~15;
            origin->triggerRight &= ~15;
            break;
        case PAD_MODE_3:
        case PAD_MODE_4:
            // Full precision mode (default)
            break;
    }
}

/*---------------------------------------------------------------------------*
  Name:         ReadKeyboard

  Description:  Read keyboard input and convert to PADStatus.
                This is a PC-specific extension not present in original.
                Allows Player 1 to play without a gamepad.
                
                On GC/Wii: N/A (no keyboard support)
                On PC: Maps keyboard to controller buttons/sticks

  Arguments:    status    Pointer to PADStatus to fill

  Returns:      None (status filled with keyboard input or error)
 *---------------------------------------------------------------------------*/
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
    
    // Get configured key bindings
    SDL_Scancode key_up = PADGetKeyboardBinding(PAD_BUTTON_UP);
    SDL_Scancode key_down = PADGetKeyboardBinding(PAD_BUTTON_DOWN);
    SDL_Scancode key_left = PADGetKeyboardBinding(PAD_BUTTON_LEFT);
    SDL_Scancode key_right = PADGetKeyboardBinding(PAD_BUTTON_RIGHT);
    
    // D-pad buttons (also used for main stick)
    if (s_keyboard.keys[key_left])  status->button |= PAD_BUTTON_LEFT;
    if (s_keyboard.keys[key_right]) status->button |= PAD_BUTTON_RIGHT;
    if (s_keyboard.keys[key_up])    status->button |= PAD_BUTTON_UP;
    if (s_keyboard.keys[key_down])  status->button |= PAD_BUTTON_DOWN;
    
    // Face buttons
    if (s_keyboard.keys[PADGetKeyboardBinding(PAD_BUTTON_A)])     status->button |= PAD_BUTTON_A;
    if (s_keyboard.keys[PADGetKeyboardBinding(PAD_BUTTON_B)])     status->button |= PAD_BUTTON_B;
    if (s_keyboard.keys[PADGetKeyboardBinding(PAD_BUTTON_X)])     status->button |= PAD_BUTTON_X;
    if (s_keyboard.keys[PADGetKeyboardBinding(PAD_BUTTON_Y)])     status->button |= PAD_BUTTON_Y;
    if (s_keyboard.keys[PADGetKeyboardBinding(PAD_BUTTON_START)]) status->button |= PAD_BUTTON_START;
    
    // Triggers (digital buttons)
    if (s_keyboard.keys[PADGetKeyboardBinding(PAD_TRIGGER_Z)])     status->button |= PAD_TRIGGER_Z;
    if (s_keyboard.keys[PADGetKeyboardBinding(PAD_TRIGGER_L)])     status->button |= PAD_TRIGGER_L;
    if (s_keyboard.keys[PADGetKeyboardBinding(PAD_TRIGGER_R)])     status->button |= PAD_TRIGGER_R;
    
    // Main analog stick (digital simulation: -100 or +100)
    // We use ±100 instead of ±127 to avoid triggering unintended actions
    if (s_keyboard.keys[key_left])  status->stickX = -100;
    if (s_keyboard.keys[key_right]) status->stickX = 100;
    if (s_keyboard.keys[key_up])    status->stickY = 100;
    if (s_keyboard.keys[key_down])  status->stickY = -100;
    
    // C-stick (digital simulation) - uses separate bindings
    if (s_keyboard.keys[KEY_CLEFT])  status->substickX = -100;
    if (s_keyboard.keys[KEY_CRIGHT]) status->substickX = 100;
    if (s_keyboard.keys[KEY_CUP])    status->substickY = 100;
    if (s_keyboard.keys[KEY_CDOWN])  status->substickY = -100;
    
    // Triggers analog (digital: 0 or 255)
    if (s_keyboard.keys[PADGetKeyboardBinding(PAD_TRIGGER_L)]) status->triggerLeft = 255;
    if (s_keyboard.keys[PADGetKeyboardBinding(PAD_TRIGGER_R)]) status->triggerRight = 255;
    
    // Analog A/B not supported on keyboard
    status->analogA = 0;
    status->analogB = 0;
}

/*---------------------------------------------------------------------------*
  Name:         ReadGamepad

  Description:  Read gamepad input via SDL2 and convert to PADStatus.
                This replaces reading SI response data on original hardware.
                
                On GC/Wii: Read 8-byte packet from SI response buffer
                On PC: Query SDL2 for button/axis state

  Arguments:    chan      Channel number to read
                status    Pointer to PADStatus to fill

  Returns:      None (status filled with gamepad input or error)
 *---------------------------------------------------------------------------*/
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
    
    // Read digital buttons (SDL_GameControllerGetButton returns 0 or 1)
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
        status->button |= PAD_BUTTON_LEFT;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
        status->button |= PAD_BUTTON_RIGHT;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP))
        status->button |= PAD_BUTTON_UP;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
        status->button |= PAD_BUTTON_DOWN;
    
    // Face buttons (SDL layout: A=cross, B=circle on PlayStation)
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
    
    // Shoulder buttons
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
        status->button |= PAD_TRIGGER_R;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
        status->button |= PAD_TRIGGER_L;
    
    // Z button (no direct equivalent - map to right stick press or back button)
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSTICK) ||
        SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK))
        status->button |= PAD_TRIGGER_Z;
    
    // Read analog stick (SDL returns -32768 to 32767, convert to -128 to 127)
    s16 axisX = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
    s16 axisY = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
    status->stickX = (s8)(axisX / 256);
    status->stickY = (s8)(-axisY / 256);  // Invert Y (SDL uses down=positive)
    
    // Read C-stick (right analog)
    s16 cAxisX = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
    s16 cAxisY = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY);
    status->substickX = (s8)(cAxisX / 256);
    status->substickY = (s8)(-cAxisY / 256);  // Invert Y
    
    // Read triggers (SDL returns 0-32767, convert to 0-255)
    u16 trigL = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    u16 trigR = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    status->triggerLeft = (u8)(trigL / 128);
    status->triggerRight = (u8)(trigR / 128);
    
    // Analog A/B not supported on modern controllers
    status->analogA = 0;
    status->analogB = 0;
    
    // Apply origin calibration (subtract baseline values)
    PADStatus* origin = &s_origin[chan];
    status->stickX -= origin->stickX;
    status->stickY -= origin->stickY;
    status->substickX -= origin->substickX;
    status->substickY -= origin->substickY;
    
    // Clamp triggers to origin (prevent negative values)
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
  Name:         PADInit

  Description:  Initializes the PAD system. Must be called before any other
                PAD functions. Initializes SDL2, enables all 4 channels, and
                starts the reset sequence.
                
                On GC/Wii: Initializes SI hardware, sets sampling rate,
                           registers reset/shutdown callbacks
                On PC: Initializes SDL2 gamepad system, scans for controllers

  Arguments:    None

  Returns:      TRUE if initialization succeeded
                FALSE if SDL2 failed to initialize
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
    
    // Reset all channels (equivalent to SIRefreshSamplingRate + PADReset in original)
    return PADReset(PAD_CHAN0_BIT | PAD_CHAN1_BIT | PAD_CHAN2_BIT | PAD_CHAN3_BIT);
}

/*---------------------------------------------------------------------------*
  Name:         PADReset

  Description:  Resets the specified game controllers. Clears origin data
                and attempts to (re)open gamepads for the specified channels.
                
                On GC/Wii: Sends reset sequence via SI (type check, origin read)
                On PC: Clears state and calls SDL_GameControllerOpen

  Arguments:    mask    Bit mask of controllers to reset (PAD_CHANn_BIT)

  Returns:      Always TRUE
 *---------------------------------------------------------------------------*/
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
            
            // Try to open gamepad (skip Player 1 if keyboard fallback active)
            if (!s_keyboard.enabled || chan != 0) {
                OpenGamepad(chan);
            }
            
            // Reset complete immediately (no async SI transfer on PC)
            s_resetting_bits &= ~chanBit;
        }
    }
    
    OSRestoreInterrupts(enabled);
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         PADRecalibrate

  Description:  Recalibrates the specified game controllers. Resets the
                origin/calibration data to recenter analog sticks and reset
                trigger baselines.
                
                On GC/Wii: Sends CMD_CALIBRATE command via SI
                On PC: Resets software origin values

  Arguments:    mask    Bit mask of controllers to recalibrate (PAD_CHANn_BIT)

  Returns:      Always TRUE
 *---------------------------------------------------------------------------*/
BOOL PADRecalibrate(u32 mask) {
    if (!s_initialized) {
        return FALSE;
    }
    
    // For PC implementation, recalibration just resets the origin.
    // In original hardware, this sends a calibration command to the controller.
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

/*---------------------------------------------------------------------------*
  Name:         PADRead

  Description:  Reads current status of all game controllers. This is the main
                function called every frame to get controller input.
                
                On GC/Wii: Reads data from SI response buffer (filled by
                           hardware DMA during automatic polling)
                On PC: Manually queries SDL2 for each controller's state

  Arguments:    status    Array of PAD_MAX_CONTROLLERS PADStatus structures
                          to fill in. Each err field indicates validity.

  Returns:      Bit mask of controllers that support rumble motors
                (PAD_CHANn_BIT for channels with rumble support)
 *---------------------------------------------------------------------------*/
u32 PADRead(PADStatus* status) {
    if (!s_initialized || !status) {
        // Initialize status array to error state
        if (status) {
            for (s32 i = 0; i < PAD_MAX_CONTROLLERS; i++) {
                status[i].err = PAD_ERR_NOT_READY;
                memset(&status[i], 0, offsetof(PADStatus, err));
            }
        }
        return 0;
    }
    
    // Pump SDL events (updates gamepad state)
    if (s_sdl_initialized) {
        SDL_PumpEvents();
    }
    
    // Scan for new gamepads (hot-plug support)
    ScanGamepads();
    
    // Call sampling callback if registered
    if (s_sampling_callback) {
        s_sampling_callback();
    }
    
    u32 motor = 0;  // Bitmask of controllers with rumble support
    
    // Read each controller
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
        
        // Read input (keyboard for chan 0 if no gamepad, else SDL2 gamepad)
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

/*---------------------------------------------------------------------------*
  Name:         PADControlMotor

  Description:  Controls the rumble motor for a single controller.
                
                On GC/Wii: Sends motor control bits via SI command packet
                           (2 bits for dual motors on some controllers)
                On PC: Uses SDL2 Haptic API for rumble effect

  Arguments:    chan      Channel number (PAD_CHANn)
                command   Motor command:
                          PAD_MOTOR_STOP - Stop rumble
                          PAD_MOTOR_RUMBLE - Start rumble
                          PAD_MOTOR_STOP_HARD - Hard stop (same as STOP on PC)

  Returns:      None
 *---------------------------------------------------------------------------*/
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
    
    // Open haptic device temporarily
    SDL_Haptic* haptic = SDL_HapticOpenFromJoystick(joy);
    if (!haptic) {
        return;
    }
    
    if (command == PAD_MOTOR_RUMBLE) {
        // Start rumble (intensity from config, 1000ms duration)
        if (SDL_HapticRumbleSupported(haptic)) {
            SDL_HapticRumbleInit(haptic);
            float intensity = PADGetRumbleIntensity();
            SDL_HapticRumblePlay(haptic, intensity, 1000);
        }
    } else {
        // Stop rumble (PAD_MOTOR_STOP or PAD_MOTOR_STOP_HARD)
        SDL_HapticRumbleStop(haptic);
    }
    
    SDL_HapticClose(haptic);
}

/*---------------------------------------------------------------------------*
  Name:         PADControlAllMotors

  Description:  Controls rumble motors for all controllers simultaneously.
                More efficient than calling PADControlMotor 4 times.
                
                On GC/Wii: Batches SI commands for all channels
                On PC: Calls PADControlMotor for each channel

  Arguments:    commandArray    Array of PAD_MAX_CONTROLLERS motor commands

  Returns:      None
 *---------------------------------------------------------------------------*/
void PADControlAllMotors(const u32* commandArray) {
    if (!s_initialized || !commandArray) {
        return;
    }
    
    for (s32 chan = 0; chan < PAD_MAX_CONTROLLERS; chan++) {
        PADControlMotor(chan, commandArray[chan]);
    }
}

/*---------------------------------------------------------------------------*
  Name:         PADGetType

  Description:  Queries game pad type information for a channel.
                
                On GC/Wii: Returns SI type register value (identifies
                           controller model, wireless/wired, motor support)
                On PC: Returns generic controller type (always same value)

  Arguments:    chan    Channel number (PAD_CHANn)
                type    Pointer to receive type information

  Returns:      TRUE if controller is connected and ready
                FALSE if not connected or resetting
 *---------------------------------------------------------------------------*/
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
    // Original would return SI type bits identifying exact hardware
    *type = 0x09000000;  // Standard controller type
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         PADSync

  Description:  Checks if PADInit() or PADReset() has completed the reset
                sequence. Used to wait for controllers to be ready.
                
                On GC/Wii: Waits for SI transfers to complete
                On PC: Checks if any channels are resetting

  Arguments:    None

  Returns:      TRUE if no reset operation is in progress
                FALSE if still resetting
 *---------------------------------------------------------------------------*/
BOOL PADSync(void) {
    return (s_resetting_bits == 0);
}

/*---------------------------------------------------------------------------*
  Name:         PADSetAnalogMode

  Description:  Sets the analog mode for all controllers. Different modes
                provide different precision for different analog inputs.
                
                On GC/Wii: Sends mode command via SI, changes data packet format
                On PC: Changes how we interpret/clamp analog values

  Arguments:    mode    Analog mode (0-7)
                        PAD_MODE_3 (default): Full stick + triggers, no analog A/B

  Returns:      None
 *---------------------------------------------------------------------------*/
void PADSetAnalogMode(u32 mode) {
    if (mode >= 8) {
        return;
    }
    
    BOOL enabled = OSDisableInterrupts();
    s_analog_mode = mode;
    OSRestoreInterrupts(enabled);
}

/*---------------------------------------------------------------------------*
  Name:         PADSetSamplingRate

  Description:  Sets the controller sampling rate in milliseconds.
                
                On GC/Wii: Configures SI hardware polling rate (affects VI timing)
                On PC: No-op - sampling rate is tied to PADRead() call frequency
                
                This function exists for API compatibility but has no effect
                on PC. Games should call PADRead() once per frame.

  Arguments:    msec    Desired sampling rate in milliseconds (ignored on PC)

  Returns:      None
 *---------------------------------------------------------------------------*/
void PADSetSamplingRate(u32 msec) {
    (void)msec;
    // No-op on PC - sampling rate tied to PADRead() calls
    // Original would configure SI SIPOLL register
}

/*---------------------------------------------------------------------------*
  Name:         PADSetSamplingCallback

  Description:  Installs a callback function that is called during PADRead()
                when controller data is sampled. Useful for custom input
                processing or recording.
                
                On GC/Wii: Called during SI polling interrupt
                On PC: Called directly in PADRead()

  Arguments:    callback    Callback function pointer, or NULL to remove

  Returns:      Previous callback function
 *---------------------------------------------------------------------------*/
PADSamplingCallback PADSetSamplingCallback(PADSamplingCallback callback) {
    PADSamplingCallback prev = s_sampling_callback;
    s_sampling_callback = callback;
    return prev;
}
