/**
 * @file VIConfig.h
 * @brief VI configuration from INI file
 */

#ifndef VI_CONFIG_H
#define VI_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dolphin/types.h>

/*---------------------------------------------------------------------------*
    Configuration Structure
 *---------------------------------------------------------------------------*/

typedef struct VIConfig {
    // Display settings
    int windowWidth;
    int windowHeight;
    BOOL fullscreen;
    BOOL maximized;
    char windowTitle[256];
    
    // Graphics settings
    int vsync;           // 0=off, 1=on, -1=adaptive
    int fpsCap;          // 0=uncapped, or specific FPS
    int msaaSamples;     // 0, 2, 4, 8, 16
    int openglMajor;
    int openglMinor;
    
    // Emulation settings
    int tvMode;          // 0=NTSC (60Hz), 1=PAL (50Hz)
    BOOL enableCallbacks;
} VIConfig;

/*---------------------------------------------------------------------------*
    Functions
 *---------------------------------------------------------------------------*/

/**
 * @brief Load VI configuration from vi_config.ini
 * 
 * @param config  Pointer to config structure to fill
 * @return TRUE if loaded successfully, FALSE on error
 */
BOOL VILoadConfig(VIConfig* config);

/**
 * @brief Get default VI configuration
 * 
 * @param config  Pointer to config structure to fill
 */
void VIGetDefaultConfig(VIConfig* config);

#ifdef __cplusplus
}
#endif

#endif // VI_CONFIG_H

