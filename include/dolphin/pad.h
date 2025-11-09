/**
 * @file pad.h
 * @brief GameCube Controller (PAD) API for libPorpoise
 * 
 * Provides controller input handling compatible with the original GameCube/Wii SDK.
 * Uses SDL2 for actual gamepad support with keyboard fallback.
 */

#ifndef DOLPHIN_PAD_H
#define DOLPHIN_PAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dolphin/types.h>

/*---------------------------------------------------------------------------*
    Constants
 *---------------------------------------------------------------------------*/

// Maximum number of controllers
#define PAD_MAX_CONTROLLERS     4

// Controller channel numbers
#define PAD_CHAN0              0
#define PAD_CHAN1              1
#define PAD_CHAN2              2
#define PAD_CHAN3              3

// Channel bit masks (for PADInit/PADReset)
#define PAD_CHAN0_BIT          0x80000000
#define PAD_CHAN1_BIT          0x40000000
#define PAD_CHAN2_BIT          0x20000000
#define PAD_CHAN3_BIT          0x10000000

// Button bits
#define PAD_BUTTON_LEFT        0x0001
#define PAD_BUTTON_RIGHT       0x0002
#define PAD_BUTTON_DOWN        0x0004
#define PAD_BUTTON_UP          0x0008
#define PAD_TRIGGER_Z          0x0010
#define PAD_TRIGGER_R          0x0020
#define PAD_TRIGGER_L          0x0040
#define PAD_BUTTON_A           0x0100
#define PAD_BUTTON_B           0x0200
#define PAD_BUTTON_X           0x0400
#define PAD_BUTTON_Y           0x0800
#define PAD_BUTTON_START       0x1000
#define PAD_BUTTON_MENU        PAD_BUTTON_START  // Alias

// Motor control commands
#define PAD_MOTOR_STOP         0
#define PAD_MOTOR_RUMBLE       1
#define PAD_MOTOR_STOP_HARD    2

// Error codes
#define PAD_ERR_NONE           0
#define PAD_ERR_NO_CONTROLLER  -1
#define PAD_ERR_NOT_READY      -2
#define PAD_ERR_TRANSFER       -3

// Analog modes
#define PAD_MODE_0             0
#define PAD_MODE_1             1
#define PAD_MODE_2             2
#define PAD_MODE_3             3  // Default - no analog A/B
#define PAD_MODE_4             4
#define PAD_MODE_5             5
#define PAD_MODE_6             6
#define PAD_MODE_7             7

// Clamp types
#define PAD_STICK_CLAMP_OCTA_WITH_MARGIN    0
#define PAD_STICK_CLAMP_OCTA_NO_MARGIN      1
#define PAD_STICK_CLAMP_CIRCLE_WITH_MARGIN  2
#define PAD_STICK_CLAMP_CIRCLE_NO_MARGIN    3
#define PAD_TRIGGER_FIXED_BASE              4
#define PAD_TRIGGER_OPEN_BASE               5

/*---------------------------------------------------------------------------*
    Types
 *---------------------------------------------------------------------------*/

/**
 * @brief Controller status structure
 * 
 * Filled in by PADRead() with current controller state.
 */
typedef struct PADStatus
{
    u16 button;           ///< Button state (PAD_BUTTON_* bits)
    s8  stickX;           ///< Main analog stick X (-128 to 127)
    s8  stickY;           ///< Main analog stick Y (-128 to 127)
    s8  substickX;        ///< C-stick X (-128 to 127)
    s8  substickY;        ///< C-stick Y (-128 to 127)
    u8  triggerLeft;      ///< L trigger analog (0-255)
    u8  triggerRight;     ///< R trigger analog (0-255)
    u8  analogA;          ///< Analog A button (0-255, usually 0)
    u8  analogB;          ///< Analog B button (0-255, usually 0)
    s8  err;              ///< Error status (PAD_ERR_*)
} PADStatus;

/**
 * @brief Sampling callback function type
 */
typedef void (*PADSamplingCallback)(void);

/*---------------------------------------------------------------------------*
    Functions
 *---------------------------------------------------------------------------*/

/**
 * @brief Initialize the PAD system
 * 
 * Must be called before any other PAD functions.
 * Initializes SDL2 gamepad subsystem and resets all controllers.
 * 
 * @return TRUE if initialization succeeded, FALSE otherwise
 */
BOOL PADInit(void);

/**
 * @brief Reset specified controllers
 * 
 * @param mask Bit mask of controllers to reset (PAD_CHANn_BIT)
 * @return TRUE if reset started successfully
 */
BOOL PADReset(u32 mask);

/**
 * @brief Recalibrate specified controllers
 * 
 * For PC implementation, this resets analog stick centers and
 * trigger baselines.
 * 
 * @param mask Bit mask of controllers to recalibrate (PAD_CHANn_BIT)
 * @return TRUE if recalibration started successfully
 */
BOOL PADRecalibrate(u32 mask);

/**
 * @brief Read controller status for all channels
 * 
 * Fills in the status array with current controller state.
 * Should be called once per frame.
 * 
 * @param status Array of PAD_MAX_CONTROLLERS PADStatus structures to fill
 * @return Bit mask of controllers that support rumble (PAD_CHANn_BIT)
 */
u32 PADRead(PADStatus* status);

/**
 * @brief Control rumble motor for a single controller
 * 
 * @param chan Channel number (PAD_CHANn)
 * @param command Motor command (PAD_MOTOR_*)
 */
void PADControlMotor(s32 chan, u32 command);

/**
 * @brief Control rumble motors for all controllers
 * 
 * @param commandArray Array of PAD_MAX_CONTROLLERS motor commands (PAD_MOTOR_*)
 */
void PADControlAllMotors(const u32* commandArray);

/**
 * @brief Clamp analog stick and trigger values
 * 
 * Applies dead zones and octagonal clamping to controller inputs.
 * Modifies status structures in place.
 * 
 * @param status Array of PAD_MAX_CONTROLLERS PADStatus structures
 */
void PADClamp(PADStatus* status);

/**
 * @brief Clamp analog sticks to circular region
 * 
 * @param status Array of PAD_MAX_CONTROLLERS PADStatus structures
 */
void PADClampCircle(PADStatus* status);

/**
 * @brief Clamp with specified parameters
 * 
 * @param status Array of PAD_MAX_CONTROLLERS PADStatus structures
 * @param type Clamp type (PAD_STICK_CLAMP_* or PAD_TRIGGER_*)
 */
void PADClamp2(PADStatus* status, u32 type);

/**
 * @brief Clamp sticks to circular region with parameters
 * 
 * @param status Array of PAD_MAX_CONTROLLERS PADStatus structures
 * @param type Clamp type (PAD_STICK_CLAMP_CIRCLE_*)
 */
void PADClampCircle2(PADStatus* status, u32 type);

/**
 * @brief Clamp trigger values with parameters
 * 
 * @param status Array of PAD_MAX_CONTROLLERS PADStatus structures  
 * @param type Clamp type (PAD_TRIGGER_*)
 */
void PADClampTrigger(PADStatus* status, u32 type);

/**
 * @brief Get controller type information
 * 
 * @param chan Channel number (PAD_CHANn)
 * @param type Pointer to receive type information
 * @return TRUE if controller is connected and ready
 */
BOOL PADGetType(s32 chan, u32* type);

/**
 * @brief Check if PADInit/PADReset has completed
 * 
 * @return TRUE if no reset operation is in progress
 */
BOOL PADSync(void);

/**
 * @brief Set analog mode for all controllers
 * 
 * Controls which analog values are available.
 * Mode 3 (default) disables analog A/B buttons.
 * 
 * @param mode Analog mode (PAD_MODE_*)
 */
void PADSetAnalogMode(u32 mode);

/**
 * @brief Set sampling rate (obsolete, for compatibility)
 * 
 * On PC, sampling is tied to frame rate via PADRead().
 * This function exists for API compatibility but has no effect.
 * 
 * @param msec Desired sampling rate in milliseconds (ignored)
 */
void PADSetSamplingRate(u32 msec);

/**
 * @brief Set sampling callback
 * 
 * Installs a callback function that is called when controller
 * data is sampled. Pass NULL to remove callback.
 * 
 * @param callback Callback function pointer
 * @return Previous callback function
 */
PADSamplingCallback PADSetSamplingCallback(PADSamplingCallback callback);

#ifdef __cplusplus
}
#endif

#endif // DOLPHIN_PAD_H

