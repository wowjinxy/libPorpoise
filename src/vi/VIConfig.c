/**
 * @file VIConfig.c
 * @brief VI configuration loader
 */

#include "VIConfig.h"
#include <dolphin/os.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*---------------------------------------------------------------------------*
  Name:         VIGetDefaultConfig

  Description:  Fill config with default values.

  Arguments:    config  Config structure to fill

  Returns:      None
 *---------------------------------------------------------------------------*/
void VIGetDefaultConfig(VIConfig* config) {
    if (!config) return;
    
    // Display defaults
    config->windowWidth = 640;
    config->windowHeight = 480;
    config->fullscreen = FALSE;
    config->maximized = FALSE;
    strcpy(config->windowTitle, "libPorpoise Game");
    
    // Graphics defaults
    config->vsync = 1;              // VSync on by default
    config->fpsCap = 60;            // 60 FPS cap
    config->msaaSamples = 0;        // No MSAA
    config->openglMajor = 3;
    config->openglMinor = 3;
    
    // Emulation defaults
    config->tvMode = 0;             // NTSC (60Hz)
    config->enableCallbacks = TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         TrimWhitespace

  Description:  Remove leading/trailing whitespace from string.

  Arguments:    str  String to trim (modified in place)

  Returns:      Pointer to trimmed string
 *---------------------------------------------------------------------------*/
static char* TrimWhitespace(char* str) {
    char* end;
    
    // Trim leading
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    // Trim trailing
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    
    return str;
}

/*---------------------------------------------------------------------------*
  Name:         ParseInt

  Description:  Parse integer from string.

  Arguments:    str  String to parse

  Returns:      Parsed integer
 *---------------------------------------------------------------------------*/
static int ParseInt(const char* str) {
    return atoi(str);
}

/*---------------------------------------------------------------------------*
  Name:         ParseBool

  Description:  Parse boolean from string (0/1, true/false, yes/no).

  Arguments:    str  String to parse

  Returns:      TRUE or FALSE
 *---------------------------------------------------------------------------*/
static BOOL ParseBool(const char* str) {
    if (strcmp(str, "1") == 0 || 
        strcmp(str, "true") == 0 || 
        strcmp(str, "TRUE") == 0 ||
        strcmp(str, "yes") == 0 ||
        strcmp(str, "YES") == 0) {
        return TRUE;
    }
    return FALSE;
}

/*---------------------------------------------------------------------------*
  Name:         VILoadConfig

  Description:  Load VI configuration from vi_config.ini.

  Arguments:    config  Config structure to fill

  Returns:      TRUE if loaded successfully, FALSE on error
 *---------------------------------------------------------------------------*/
BOOL VILoadConfig(VIConfig* config) {
    FILE* file;
    char line[512];
    char section[64] = "";
    char* key;
    char* value;
    char* equals;
    
    if (!config) return FALSE;
    
    // Start with defaults
    VIGetDefaultConfig(config);
    
    // Open config file
    file = fopen("vi_config.ini", "r");
    if (!file) {
        OSReport("VI: vi_config.ini not found, using defaults\n");
        return FALSE;
    }
    
    OSReport("VI: Loading configuration from vi_config.ini\n");
    
    // Parse line by line
    while (fgets(line, sizeof(line), file)) {
        char* trimmed = TrimWhitespace(line);
        
        // Skip empty lines and comments
        if (trimmed[0] == '\0' || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }
        
        // Check for section header
        if (trimmed[0] == '[') {
            char* end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                strncpy(section, trimmed + 1, sizeof(section) - 1);
                section[sizeof(section) - 1] = '\0';
            }
            continue;
        }
        
        // Parse key=value
        equals = strchr(trimmed, '=');
        if (!equals) continue;
        
        *equals = '\0';
        key = TrimWhitespace(trimmed);
        value = TrimWhitespace(equals + 1);
        
        // Parse based on section and key
        if (strcmp(section, "Display") == 0) {
            if (strcmp(key, "width") == 0) {
                config->windowWidth = ParseInt(value);
            } else if (strcmp(key, "height") == 0) {
                config->windowHeight = ParseInt(value);
            } else if (strcmp(key, "fullscreen") == 0) {
                config->fullscreen = ParseBool(value);
            } else if (strcmp(key, "maximized") == 0) {
                config->maximized = ParseBool(value);
            } else if (strcmp(key, "title") == 0) {
                strncpy(config->windowTitle, value, sizeof(config->windowTitle) - 1);
                config->windowTitle[sizeof(config->windowTitle) - 1] = '\0';
            }
        }
        else if (strcmp(section, "Graphics") == 0) {
            if (strcmp(key, "vsync") == 0) {
                config->vsync = ParseInt(value);
            } else if (strcmp(key, "fps_cap") == 0) {
                config->fpsCap = ParseInt(value);
            } else if (strcmp(key, "msaa_samples") == 0) {
                config->msaaSamples = ParseInt(value);
            } else if (strcmp(key, "opengl_major") == 0) {
                config->openglMajor = ParseInt(value);
            } else if (strcmp(key, "opengl_minor") == 0) {
                config->openglMinor = ParseInt(value);
            }
        }
        else if (strcmp(section, "Emulation") == 0) {
            if (strcmp(key, "tv_mode") == 0) {
                if (strcmp(value, "PAL") == 0) {
                    config->tvMode = 1;  // PAL (50Hz)
                } else {
                    config->tvMode = 0;  // NTSC (60Hz)
                }
            } else if (strcmp(key, "enable_callbacks") == 0) {
                config->enableCallbacks = ParseBool(value);
            }
        }
    }
    
    fclose(file);
    
    // Log loaded config
    OSReport("VI: Configuration loaded:\n");
    OSReport("  Window: %dx%d %s\n", 
             config->windowWidth, config->windowHeight,
             config->fullscreen ? "(fullscreen)" : "(windowed)");
    OSReport("  VSync: %s\n", 
             config->vsync == 1 ? "On" : (config->vsync == -1 ? "Adaptive" : "Off"));
    if (config->fpsCap == 0) {
        OSReport("  FPS Cap: Uncapped\n");
    } else {
        OSReport("  FPS Cap: %d\n", config->fpsCap);
    }
    OSReport("  TV Mode: %s\n", 
             config->tvMode == 1 ? "PAL (50Hz)" : "NTSC (60Hz)");
    
    return TRUE;
}

