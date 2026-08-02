#include <dolphin/pad.h>
#include <dolphin/PADConfig.h>
#include <dolphin/os.h>
#ifdef LIBPORPOISE_PORT
#include <simulator/sim_host_Benchmark.h>
#endif

#include <SDL2/SDL.h>
#include <stddef.h>
#include <string.h>

static BOOL Initialized = FALSE;
static u32 EnabledBits = 0;
static u32 AnalogMode = PAD_MODE_3;
static PADSamplingCallback SamplingCallback = NULL;
static SDL_GameController* Controllers[PAD_MAX_CONTROLLERS];
static SDL_JoystickID ControllerIds[PAD_MAX_CONTROLLERS] = {
    -1, -1, -1, -1
};

u32 __PADSpec = PAD_SPEC_5;

static s8 ClampAxisValue(int value)
{
    if (value < -128) {
        return -128;
    }
    if (value > 127) {
        return 127;
    }
    return (s8)value;
}

static s8 ConvertAxis(Sint16 raw, int deadzone, float sensitivity, BOOL invert)
{
    int value = raw / 256;
    if (invert) {
        value = -value;
    }
    if (value > -deadzone && value < deadzone) {
        return 0;
    }
    return ClampAxisValue((int)((float)value * sensitivity));
}

static u8 ConvertTrigger(Sint16 raw)
{
    int value = raw;
    if (value < 0) {
        value = 0;
    }
    value /= 128;
    if (value > 255) {
        value = 255;
    }
    return (u8)value;
}

static BOOL IsControllerOpen(SDL_JoystickID id)
{
    s32 chan;
    for (chan = 0; chan < PAD_MAX_CONTROLLERS; ++chan) {
        if (Controllers[chan] != NULL && ControllerIds[chan] == id) {
            return TRUE;
        }
    }
    return FALSE;
}

static void CloseDetachedControllers(void)
{
    s32 chan;
    for (chan = 0; chan < PAD_MAX_CONTROLLERS; ++chan) {
        if (Controllers[chan] != NULL &&
            !SDL_GameControllerGetAttached(Controllers[chan])) {
            OSReport("PAD: Channel %d disconnected\n", chan);
            SDL_GameControllerClose(Controllers[chan]);
            Controllers[chan] = NULL;
            ControllerIds[chan] = -1;
        }
    }
}

static void ScanControllers(void)
{
    int deviceIndex;
    CloseDetachedControllers();

    for (deviceIndex = 0; deviceIndex < SDL_NumJoysticks(); ++deviceIndex) {
        SDL_GameController* controller;
        SDL_Joystick* joystick;
        SDL_JoystickID id;
        s32 chan;

        if (!SDL_IsGameController(deviceIndex)) {
            continue;
        }
        id = SDL_JoystickGetDeviceInstanceID(deviceIndex);
        if (id < 0 || IsControllerOpen(id)) {
            continue;
        }

        for (chan = 0; chan < PAD_MAX_CONTROLLERS; ++chan) {
            if (Controllers[chan] == NULL &&
                (EnabledBits & (PAD_CHAN0_BIT >> chan)) != 0) {
                break;
            }
        }
        if (chan == PAD_MAX_CONTROLLERS) {
            break;
        }

        controller = SDL_GameControllerOpen(deviceIndex);
        if (controller == NULL) {
            continue;
        }
        joystick = SDL_GameControllerGetJoystick(controller);
        Controllers[chan] = controller;
        ControllerIds[chan] = SDL_JoystickInstanceID(joystick);
        OSReport(
            "PAD: Channel %d connected - %s\n",
            chan,
            SDL_GameControllerName(controller));
    }
}

static BOOL ControllerButton(
    SDL_GameController* controller,
    u16 gcButton,
    SDL_GameControllerButton defaultButton)
{
    int configured = PADGetGamepadMapping(gcButton);
    const SDL_GameControllerButton button =
        configured >= 0
            ? (SDL_GameControllerButton)configured
            : defaultButton;
    return SDL_GameControllerGetButton(controller, button) != 0;
}

static void ReadKeyboard(PADStatus* status)
{
    const Uint8* keys;
    const SDL_Scancode keyUp = PADGetKeyboardBinding(PAD_BUTTON_UP);
    const SDL_Scancode keyDown = PADGetKeyboardBinding(PAD_BUTTON_DOWN);
    const SDL_Scancode keyLeft = PADGetKeyboardBinding(PAD_BUTTON_LEFT);
    const SDL_Scancode keyRight = PADGetKeyboardBinding(PAD_BUTTON_RIGHT);
    const SDL_Scancode keyL = PADGetKeyboardBinding(PAD_TRIGGER_L);
    const SDL_Scancode keyR = PADGetKeyboardBinding(PAD_TRIGGER_R);

    keys = SDL_GetKeyboardState(NULL);
    memset(status, 0, sizeof(*status));
    status->err = PAD_ERR_NONE;

    if (keys[keyLeft]) {
        status->button |= PAD_BUTTON_LEFT;
        status->stickX = -100;
    }
    if (keys[keyRight]) {
        status->button |= PAD_BUTTON_RIGHT;
        status->stickX = 100;
    }
    if (keys[keyUp]) {
        status->button |= PAD_BUTTON_UP;
        status->stickY = 100;
    }
    if (keys[keyDown]) {
        status->button |= PAD_BUTTON_DOWN;
        status->stickY = -100;
    }
    if (keys[PADGetKeyboardBinding(PAD_BUTTON_A)]) {
        status->button |= PAD_BUTTON_A;
        status->analogA = 255;
    }
    if (keys[PADGetKeyboardBinding(PAD_BUTTON_B)]) {
        status->button |= PAD_BUTTON_B;
        status->analogB = 255;
    }
    if (keys[PADGetKeyboardBinding(PAD_BUTTON_X)]) {
        status->button |= PAD_BUTTON_X;
    }
    if (keys[PADGetKeyboardBinding(PAD_BUTTON_Y)]) {
        status->button |= PAD_BUTTON_Y;
    }
    if (keys[PADGetKeyboardBinding(PAD_BUTTON_START)]) {
        status->button |= PAD_BUTTON_START;
    }
    if (keys[PADGetKeyboardBinding(PAD_TRIGGER_Z)]) {
        status->button |= PAD_TRIGGER_Z;
    }
    if (keys[keyL]) {
        status->button |= PAD_TRIGGER_L;
        status->triggerLeft = 255;
    }
    if (keys[keyR]) {
        status->button |= PAD_TRIGGER_R;
        status->triggerRight = 255;
    }

    if (keys[SDL_SCANCODE_J]) {
        status->substickX = -100;
    }
    if (keys[SDL_SCANCODE_L]) {
        status->substickX = 100;
    }
    if (keys[SDL_SCANCODE_I]) {
        status->substickY = 100;
    }
    if (keys[SDL_SCANCODE_K]) {
        status->substickY = -100;
    }
}

void __PADHostMergeKeyboardState(
    PADStatus* status, const PADStatus* keyboard)
{
    if (status == NULL || keyboard == NULL) {
        return;
    }

    status->button |= keyboard->button;

    if (keyboard->stickX != 0) {
        status->stickX = keyboard->stickX;
    }
    if (keyboard->stickY != 0) {
        status->stickY = keyboard->stickY;
    }
    if (keyboard->substickX != 0) {
        status->substickX = keyboard->substickX;
    }
    if (keyboard->substickY != 0) {
        status->substickY = keyboard->substickY;
    }

    if (keyboard->triggerLeft > status->triggerLeft) {
        status->triggerLeft = keyboard->triggerLeft;
    }
    if (keyboard->triggerRight > status->triggerRight) {
        status->triggerRight = keyboard->triggerRight;
    }
    if (keyboard->analogA > status->analogA) {
        status->analogA = keyboard->analogA;
    }
    if (keyboard->analogB > status->analogB) {
        status->analogB = keyboard->analogB;
    }
}

static void ReadController(s32 chan, PADStatus* status)
{
    SDL_GameController* controller = Controllers[chan];
    const int stickDeadzone = PADGetDeadzone(0);
    const int cStickDeadzone = PADGetDeadzone(1);
    const int triggerDeadzone = PADGetDeadzone(2);
    const float stickSensitivity = PADGetSensitivity(FALSE);
    const float cStickSensitivity = PADGetSensitivity(TRUE);

    memset(status, 0, sizeof(*status));
    if (controller == NULL) {
        status->err = PAD_ERR_NO_CONTROLLER;
        return;
    }
    status->err = PAD_ERR_NONE;

    if (SDL_GameControllerGetButton(
            controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {
        status->button |= PAD_BUTTON_LEFT;
    }
    if (SDL_GameControllerGetButton(
            controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
        status->button |= PAD_BUTTON_RIGHT;
    }
    if (SDL_GameControllerGetButton(
            controller, SDL_CONTROLLER_BUTTON_DPAD_UP)) {
        status->button |= PAD_BUTTON_UP;
    }
    if (SDL_GameControllerGetButton(
            controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {
        status->button |= PAD_BUTTON_DOWN;
    }
    if (ControllerButton(
            controller, PAD_BUTTON_A, SDL_CONTROLLER_BUTTON_A)) {
        status->button |= PAD_BUTTON_A;
        status->analogA = 255;
    }
    if (ControllerButton(
            controller, PAD_BUTTON_B, SDL_CONTROLLER_BUTTON_B)) {
        status->button |= PAD_BUTTON_B;
        status->analogB = 255;
    }
    if (ControllerButton(
            controller, PAD_BUTTON_X, SDL_CONTROLLER_BUTTON_X)) {
        status->button |= PAD_BUTTON_X;
    }
    if (ControllerButton(
            controller, PAD_BUTTON_Y, SDL_CONTROLLER_BUTTON_Y)) {
        status->button |= PAD_BUTTON_Y;
    }
    if (ControllerButton(
            controller, PAD_BUTTON_START, SDL_CONTROLLER_BUTTON_START)) {
        status->button |= PAD_BUTTON_START;
    }

    status->stickX = ConvertAxis(
        SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX),
        stickDeadzone,
        stickSensitivity,
        FALSE);
    status->stickY = ConvertAxis(
        SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY),
        stickDeadzone,
        stickSensitivity,
        TRUE);
    status->substickX = ConvertAxis(
        SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX),
        cStickDeadzone,
        cStickSensitivity,
        FALSE);
    status->substickY = ConvertAxis(
        SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY),
        cStickDeadzone,
        cStickSensitivity,
        TRUE);
    status->triggerLeft = ConvertTrigger(
        SDL_GameControllerGetAxis(
            controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT));
    status->triggerRight = ConvertTrigger(
        SDL_GameControllerGetAxis(
            controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT));

    if (ControllerButton(
            controller, PAD_TRIGGER_L, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) ||
        status->triggerLeft > triggerDeadzone) {
        status->button |= PAD_TRIGGER_L;
    }
    if (ControllerButton(
            controller, PAD_TRIGGER_R, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) ||
        status->triggerRight > triggerDeadzone) {
        status->button |= PAD_TRIGGER_R;
    }

    {
        const int configuredZ = PADGetGamepadMapping(PAD_TRIGGER_Z);
        if ((configuredZ >= 0 &&
             SDL_GameControllerGetButton(
                 controller, (SDL_GameControllerButton)configuredZ)) ||
            (configuredZ < 0 &&
             (SDL_GameControllerGetButton(
                  controller, SDL_CONTROLLER_BUTTON_RIGHTSTICK) ||
              SDL_GameControllerGetButton(
                  controller, SDL_CONTROLLER_BUTTON_BACK)))) {
            status->button |= PAD_TRIGGER_Z;
        }
    }
}

BOOL PADInit(void)
{
    if (Initialized) {
        return TRUE;
    }

    if (SDL_InitSubSystem(
            SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_EVENTS) < 0) {
        OSReport("PAD: SDL initialization failed: %s\n", SDL_GetError());
        return FALSE;
    }
    SDL_GameControllerEventState(SDL_ENABLE);
    PADLoadConfig();
    memset(Controllers, 0, sizeof(Controllers));
    Initialized = TRUE;
    return PADReset(
        PAD_CHAN0_BIT | PAD_CHAN1_BIT | PAD_CHAN2_BIT | PAD_CHAN3_BIT);
}

BOOL PADReset(u32 mask)
{
    if (!Initialized) {
        return FALSE;
    }
    EnabledBits |= mask;
    ScanControllers();
    return TRUE;
}

BOOL PADRecalibrate(u32 mask)
{
    (void)mask;
    return Initialized;
}

u32 PADRead(PADStatus* status)
{
    u32 motorBits = 0;
    s32 chan;

    if (status == NULL) {
        return 0;
    }
    if (!Initialized && !PADInit()) {
        for (chan = 0; chan < PAD_MAX_CONTROLLERS; ++chan) {
            memset(&status[chan], 0, sizeof(status[chan]));
            status[chan].err = PAD_ERR_NOT_READY;
        }
        return 0;
    }
#ifdef LIBPORPOISE_PORT
    if (SIM_HostBenchmarkNeutralInput()) {
        for (chan = 0; chan < PAD_MAX_CONTROLLERS; ++chan) {
            memset(&status[chan], 0, sizeof(status[chan]));
            status[chan].err =
                chan == PAD_CHAN0 ? PAD_ERR_NONE : PAD_ERR_NO_CONTROLLER;
        }
        if (SamplingCallback != NULL) {
            SamplingCallback();
        }
        return 0;
    }
#endif

    ScanControllers();
    if (SamplingCallback != NULL) {
        SamplingCallback();
    }

    for (chan = 0; chan < PAD_MAX_CONTROLLERS; ++chan) {
        const u32 chanBit = PAD_CHAN0_BIT >> chan;
        if ((EnabledBits & chanBit) == 0) {
            memset(&status[chan], 0, sizeof(status[chan]));
            status[chan].err = PAD_ERR_NO_CONTROLLER;
        } else if (Controllers[chan] != NULL) {
            ReadController(chan, &status[chan]);
            if (chan == PAD_CHAN0) {
                PADStatus keyboard;
                ReadKeyboard(&keyboard);
                __PADHostMergeKeyboardState(&status[chan], &keyboard);
            }
            motorBits |= chanBit;
        } else if (chan == PAD_CHAN0) {
            ReadKeyboard(&status[chan]);
        } else {
            memset(&status[chan], 0, sizeof(status[chan]));
            status[chan].err = PAD_ERR_NO_CONTROLLER;
        }
    }
    return motorBits;
}

void PADControlMotor(s32 chan, u32 command)
{
    SDL_GameController* controller;
    u16 strength;

    if (chan < 0 || chan >= PAD_MAX_CONTROLLERS) {
        return;
    }
    controller = Controllers[chan];
    if (controller == NULL) {
        return;
    }
    if (command == PAD_MOTOR_RUMBLE) {
        const float intensity = PADGetRumbleIntensity();
        strength = (u16)(intensity * 65535.0f);
        SDL_GameControllerRumble(
            controller, strength, strength, 0xffffffffu);
    } else {
        SDL_GameControllerRumble(controller, 0, 0, 0);
    }
}

void PADControlAllMotors(const u32* commandArray)
{
    s32 chan;
    if (commandArray == NULL) {
        return;
    }
    for (chan = 0; chan < PAD_MAX_CONTROLLERS; ++chan) {
        PADControlMotor(chan, commandArray[chan]);
    }
}

int PADGetType(s32 chan, u32* type)
{
    if (!Initialized ||
        type == NULL ||
        chan < 0 ||
        chan >= PAD_MAX_CONTROLLERS ||
        (Controllers[chan] == NULL && chan != PAD_CHAN0)) {
        return FALSE;
    }
    *type = 0x09000000u;
    return TRUE;
}

BOOL PADSync(void)
{
    return Initialized;
}

void PADSetAnalogMode(u32 mode)
{
    if (mode <= PAD_MODE_7) {
        AnalogMode = mode;
    }
}

void PADSetSpec(u32 spec)
{
    __PADSpec = spec;
}

u32 PADGetSpec(void)
{
    return __PADSpec;
}

void PADSetSamplingRate(u32 msec)
{
    (void)msec;
}

#if OS_BUILD_VERSION >= 20011112L
PADSamplingCallback PADSetSamplingCallback(PADSamplingCallback callback)
{
    PADSamplingCallback previous = SamplingCallback;
    SamplingCallback = callback;
    return previous;
}
#else
void PADSetSamplingCallback(PADSamplingCallback callback)
{
    SamplingCallback = callback;
}
#endif
