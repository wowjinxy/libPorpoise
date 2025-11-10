/**
 * @file PADConfig.h
 * @brief Internal header for PAD configuration system
 * 
 * Not part of public API - used internally by PAD module.
 */

#ifndef PAD_CONFIG_H
#define PAD_CONFIG_H

#include <dolphin/types.h>
#include <SDL.h>

/**
 * @brief Load configuration from pad_config.ini
 * @return TRUE if loaded successfully, FALSE if using defaults
 */
BOOL PADLoadConfig(void);

/**
 * @brief Save current configuration to pad_config.ini
 * @return TRUE if saved successfully
 */
BOOL PADSaveConfig(void);

/**
 * @brief Get keyboard scancode for a GC button
 * @param button GC button (PAD_BUTTON_* or PAD_TRIGGER_*)
 * @return SDL_Scancode
 */
SDL_Scancode PADGetKeyboardBinding(u16 button);

/**
 * @brief Get SDL button mapping for a GC button
 * @param button GC button (PAD_BUTTON_* or PAD_TRIGGER_*)
 * @return SDL_GameControllerButton or -1 for default
 */
int PADGetGamepadMapping(u16 button);

/**
 * @brief Get dead zone value
 * @param type 0=main stick, 1=C-stick, 2=triggers
 * @return Dead zone value (0-127)
 */
int PADGetDeadzone(int type);

/**
 * @brief Get sensitivity multiplier
 * @param is_c_stick TRUE for C-stick, FALSE for main stick
 * @return Sensitivity multiplier (0.1 to 2.0)
 */
float PADGetSensitivity(BOOL is_c_stick);

/**
 * @brief Get rumble intensity
 * @return Rumble intensity (0.0 to 1.0)
 */
float PADGetRumbleIntensity(void);

#endif // PAD_CONFIG_H

