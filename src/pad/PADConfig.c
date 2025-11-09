/*---------------------------------------------------------------------------*
  PADConfig.c - Controller Configuration System
  
  Loads controller settings from "pad_config.ini" file, allowing users to:
  - Rebind keyboard keys
  - Remap gamepad buttons
  - Adjust analog dead zones
  - Configure sensitivity
  - Set rumble intensity
  
  Config file format (pad_config.ini):
  
  [Keyboard]
  ; Arrow keys / Main stick
  up=Up
  down=Down
  left=Left
  right=Right
  
  ; Face buttons
  a=Z
  b=X
  x=C
  y=V
  start=Return
  
  ; Triggers
  l=A
  r=S
  z=D
  
  ; C-stick
  c_up=I
  c_down=K
  c_left=J
  c_right=L
  
  [Gamepad]
  ; Button remapping (swap buttons if needed)
  ; Format: gcbutton=sdlbutton
  ; Example: a=b (map A button to SDL B button)
  
  [Settings]
  stick_deadzone=15
  c_stick_deadzone=15
  trigger_deadzone=30
  stick_sensitivity=1.0
  c_stick_sensitivity=1.0
  rumble_intensity=0.5
  
 *---------------------------------------------------------------------------*/

#include <dolphin/pad.h>
#include <dolphin/os.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <SDL2/SDL.h>

/*---------------------------------------------------------------------------*
    Configuration Structure
 *---------------------------------------------------------------------------*/

typedef struct PADConfig {
    // Keyboard bindings (SDL scancodes)
    SDL_Scancode kb_up;
    SDL_Scancode kb_down;
    SDL_Scancode kb_left;
    SDL_Scancode kb_right;
    SDL_Scancode kb_a;
    SDL_Scancode kb_b;
    SDL_Scancode kb_x;
    SDL_Scancode kb_y;
    SDL_Scancode kb_start;
    SDL_Scancode kb_l;
    SDL_Scancode kb_r;
    SDL_Scancode kb_z;
    SDL_Scancode kb_c_up;
    SDL_Scancode kb_c_down;
    SDL_Scancode kb_c_left;
    SDL_Scancode kb_c_right;
    
    // Gamepad button remapping (SDL button IDs)
    // -1 means use default mapping
    int gp_a;
    int gp_b;
    int gp_x;
    int gp_y;
    int gp_start;
    int gp_l;
    int gp_r;
    int gp_z;
    
    // Dead zones (0-127)
    int stick_deadzone;
    int c_stick_deadzone;
    int trigger_deadzone;
    
    // Sensitivity multipliers (0.1 to 2.0)
    float stick_sensitivity;
    float c_stick_sensitivity;
    
    // Rumble intensity (0.0 to 1.0)
    float rumble_intensity;
    
} PADConfig;

// Global configuration
static PADConfig s_config;
static BOOL s_config_loaded = FALSE;

/*---------------------------------------------------------------------------*
    Default Configuration
 *---------------------------------------------------------------------------*/

static const PADConfig s_default_config = {
    // Default keyboard bindings
    .kb_up = SDL_SCANCODE_UP,
    .kb_down = SDL_SCANCODE_DOWN,
    .kb_left = SDL_SCANCODE_LEFT,
    .kb_right = SDL_SCANCODE_RIGHT,
    .kb_a = SDL_SCANCODE_Z,
    .kb_b = SDL_SCANCODE_X,
    .kb_x = SDL_SCANCODE_C,
    .kb_y = SDL_SCANCODE_V,
    .kb_start = SDL_SCANCODE_RETURN,
    .kb_l = SDL_SCANCODE_A,
    .kb_r = SDL_SCANCODE_S,
    .kb_z = SDL_SCANCODE_D,
    .kb_c_up = SDL_SCANCODE_I,
    .kb_c_down = SDL_SCANCODE_K,
    .kb_c_left = SDL_SCANCODE_J,
    .kb_c_right = SDL_SCANCODE_L,
    
    // Default gamepad mappings (-1 = use SDL default)
    .gp_a = -1,
    .gp_b = -1,
    .gp_x = -1,
    .gp_y = -1,
    .gp_start = -1,
    .gp_l = -1,
    .gp_r = -1,
    .gp_z = -1,
    
    // Default dead zones
    .stick_deadzone = 15,
    .c_stick_deadzone = 15,
    .trigger_deadzone = 30,
    
    // Default sensitivity
    .stick_sensitivity = 1.0f,
    .c_stick_sensitivity = 1.0f,
    
    // Default rumble
    .rumble_intensity = 0.5f,
};

/*---------------------------------------------------------------------------*
    Helper Functions
 *---------------------------------------------------------------------------*/

/**
 * @brief Trim whitespace from string
 */
static char* trim(char* str) {
    char* end;
    
    // Trim leading space
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    
    end[1] = '\0';
    return str;
}

/**
 * @brief Get SDL scancode from key name
 */
static SDL_Scancode GetScancodeFromName(const char* name) {
    // Convert to SDL scancode name format (SDL_SCANCODE_*)
    char sdl_name[64];
    snprintf(sdl_name, sizeof(sdl_name), "%s", name);
    
    // Convert to uppercase
    for (int i = 0; sdl_name[i]; i++) {
        sdl_name[i] = toupper(sdl_name[i]);
    }
    
    return SDL_GetScancodeFromName(sdl_name);
}

/**
 * @brief Get SDL controller button from name
 */
static int GetButtonFromName(const char* name) {
    // Map common button names to SDL_GameControllerButton enum
    if (strcasecmp(name, "a") == 0) return SDL_CONTROLLER_BUTTON_A;
    if (strcasecmp(name, "b") == 0) return SDL_CONTROLLER_BUTTON_B;
    if (strcasecmp(name, "x") == 0) return SDL_CONTROLLER_BUTTON_X;
    if (strcasecmp(name, "y") == 0) return SDL_CONTROLLER_BUTTON_Y;
    if (strcasecmp(name, "start") == 0) return SDL_CONTROLLER_BUTTON_START;
    if (strcasecmp(name, "back") == 0) return SDL_CONTROLLER_BUTTON_BACK;
    if (strcasecmp(name, "guide") == 0) return SDL_CONTROLLER_BUTTON_GUIDE;
    if (strcasecmp(name, "leftshoulder") == 0) return SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    if (strcasecmp(name, "rightshoulder") == 0) return SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    if (strcasecmp(name, "leftstick") == 0) return SDL_CONTROLLER_BUTTON_LEFTSTICK;
    if (strcasecmp(name, "rightstick") == 0) return SDL_CONTROLLER_BUTTON_RIGHTSTICK;
    
    return -1;
}

/*---------------------------------------------------------------------------*
  Name:         PADLoadConfig

  Description:  Loads configuration from pad_config.ini file. If file doesn't
                exist or has errors, uses default configuration.

  Arguments:    None

  Returns:      TRUE if config loaded successfully, FALSE if using defaults
 *---------------------------------------------------------------------------*/
BOOL PADLoadConfig(void) {
    FILE* file;
    char line[256];
    char section[64] = "";
    
    // Start with defaults
    s_config = s_default_config;
    s_config_loaded = TRUE;
    
    // Try to open config file
    file = fopen("pad_config.ini", "r");
    if (!file) {
        OSReport("PAD: Config file not found, using defaults\n");
        return FALSE;
    }
    
    OSReport("PAD: Loading configuration from pad_config.ini\n");
    
    // Parse INI file
    while (fgets(line, sizeof(line), file)) {
        char* trimmed = trim(line);
        
        // Skip empty lines and comments
        if (trimmed[0] == '\0' || trimmed[0] == ';' || trimmed[0] == '#') {
            continue;
        }
        
        // Check for section header
        if (trimmed[0] == '[') {
            char* end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                strncpy(section, trimmed + 1, sizeof(section) - 1);
                continue;
            }
        }
        
        // Parse key=value pairs
        char* equals = strchr(trimmed, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char* key = trim(trimmed);
        char* value = trim(equals + 1);
        
        // [Keyboard] section
        if (strcmp(section, "Keyboard") == 0) {
            SDL_Scancode scancode = GetScancodeFromName(value);
            if (scancode != SDL_SCANCODE_UNKNOWN) {
                if (strcmp(key, "up") == 0) s_config.kb_up = scancode;
                else if (strcmp(key, "down") == 0) s_config.kb_down = scancode;
                else if (strcmp(key, "left") == 0) s_config.kb_left = scancode;
                else if (strcmp(key, "right") == 0) s_config.kb_right = scancode;
                else if (strcmp(key, "a") == 0) s_config.kb_a = scancode;
                else if (strcmp(key, "b") == 0) s_config.kb_b = scancode;
                else if (strcmp(key, "x") == 0) s_config.kb_x = scancode;
                else if (strcmp(key, "y") == 0) s_config.kb_y = scancode;
                else if (strcmp(key, "start") == 0) s_config.kb_start = scancode;
                else if (strcmp(key, "l") == 0) s_config.kb_l = scancode;
                else if (strcmp(key, "r") == 0) s_config.kb_r = scancode;
                else if (strcmp(key, "z") == 0) s_config.kb_z = scancode;
                else if (strcmp(key, "c_up") == 0) s_config.kb_c_up = scancode;
                else if (strcmp(key, "c_down") == 0) s_config.kb_c_down = scancode;
                else if (strcmp(key, "c_left") == 0) s_config.kb_c_left = scancode;
                else if (strcmp(key, "c_right") == 0) s_config.kb_c_right = scancode;
            }
        }
        // [Gamepad] section
        else if (strcmp(section, "Gamepad") == 0) {
            int button = GetButtonFromName(value);
            if (strcmp(key, "a") == 0) s_config.gp_a = button;
            else if (strcmp(key, "b") == 0) s_config.gp_b = button;
            else if (strcmp(key, "x") == 0) s_config.gp_x = button;
            else if (strcmp(key, "y") == 0) s_config.gp_y = button;
            else if (strcmp(key, "start") == 0) s_config.gp_start = button;
            else if (strcmp(key, "l") == 0) s_config.gp_l = button;
            else if (strcmp(key, "r") == 0) s_config.gp_r = button;
            else if (strcmp(key, "z") == 0) s_config.gp_z = button;
        }
        // [Settings] section
        else if (strcmp(section, "Settings") == 0) {
            if (strcmp(key, "stick_deadzone") == 0) {
                s_config.stick_deadzone = atoi(value);
            } else if (strcmp(key, "c_stick_deadzone") == 0) {
                s_config.c_stick_deadzone = atoi(value);
            } else if (strcmp(key, "trigger_deadzone") == 0) {
                s_config.trigger_deadzone = atoi(value);
            } else if (strcmp(key, "stick_sensitivity") == 0) {
                s_config.stick_sensitivity = (float)atof(value);
            } else if (strcmp(key, "c_stick_sensitivity") == 0) {
                s_config.c_stick_sensitivity = (float)atof(value);
            } else if (strcmp(key, "rumble_intensity") == 0) {
                s_config.rumble_intensity = (float)atof(value);
            }
        }
    }
    
    fclose(file);
    
    OSReport("PAD: Configuration loaded successfully\n");
    OSReport("  Stick deadzone: %d\n", s_config.stick_deadzone);
    OSReport("  C-stick deadzone: %d\n", s_config.c_stick_deadzone);
    OSReport("  Trigger deadzone: %d\n", s_config.trigger_deadzone);
    OSReport("  Stick sensitivity: %.2f\n", s_config.stick_sensitivity);
    OSReport("  Rumble intensity: %.2f\n", s_config.rumble_intensity);
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         PADSaveConfig

  Description:  Saves current configuration to pad_config.ini file.

  Arguments:    None

  Returns:      TRUE if saved successfully
 *---------------------------------------------------------------------------*/
BOOL PADSaveConfig(void) {
    FILE* file = fopen("pad_config.ini", "w");
    if (!file) {
        OSReport("PAD: Failed to save config file\n");
        return FALSE;
    }
    
    fprintf(file, "; libPorpoise PAD Configuration\n");
    fprintf(file, "; Auto-generated config file\n\n");
    
    fprintf(file, "[Keyboard]\n");
    fprintf(file, "; Main stick / D-pad\n");
    fprintf(file, "up=%s\n", SDL_GetScancodeName(s_config.kb_up));
    fprintf(file, "down=%s\n", SDL_GetScancodeName(s_config.kb_down));
    fprintf(file, "left=%s\n", SDL_GetScancodeName(s_config.kb_left));
    fprintf(file, "right=%s\n", SDL_GetScancodeName(s_config.kb_right));
    fprintf(file, "\n; Face buttons\n");
    fprintf(file, "a=%s\n", SDL_GetScancodeName(s_config.kb_a));
    fprintf(file, "b=%s\n", SDL_GetScancodeName(s_config.kb_b));
    fprintf(file, "x=%s\n", SDL_GetScancodeName(s_config.kb_x));
    fprintf(file, "y=%s\n", SDL_GetScancodeName(s_config.kb_y));
    fprintf(file, "start=%s\n", SDL_GetScancodeName(s_config.kb_start));
    fprintf(file, "\n; Triggers\n");
    fprintf(file, "l=%s\n", SDL_GetScancodeName(s_config.kb_l));
    fprintf(file, "r=%s\n", SDL_GetScancodeName(s_config.kb_r));
    fprintf(file, "z=%s\n", SDL_GetScancodeName(s_config.kb_z));
    fprintf(file, "\n; C-stick\n");
    fprintf(file, "c_up=%s\n", SDL_GetScancodeName(s_config.kb_c_up));
    fprintf(file, "c_down=%s\n", SDL_GetScancodeName(s_config.kb_c_down));
    fprintf(file, "c_left=%s\n", SDL_GetScancodeName(s_config.kb_c_left));
    fprintf(file, "c_right=%s\n", SDL_GetScancodeName(s_config.kb_c_right));
    
    fprintf(file, "\n[Settings]\n");
    fprintf(file, "stick_deadzone=%d\n", s_config.stick_deadzone);
    fprintf(file, "c_stick_deadzone=%d\n", s_config.c_stick_deadzone);
    fprintf(file, "trigger_deadzone=%d\n", s_config.trigger_deadzone);
    fprintf(file, "stick_sensitivity=%.2f\n", s_config.stick_sensitivity);
    fprintf(file, "c_stick_sensitivity=%.2f\n", s_config.c_stick_sensitivity);
    fprintf(file, "rumble_intensity=%.2f\n", s_config.rumble_intensity);
    
    fclose(file);
    OSReport("PAD: Configuration saved to pad_config.ini\n");
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         PADGetConfig

  Description:  Gets the current configuration. If not loaded, loads it first.

  Arguments:    None

  Returns:      Pointer to current configuration
 *---------------------------------------------------------------------------*/
const PADConfig* PADGetConfig(void) {
    if (!s_config_loaded) {
        PADLoadConfig();
    }
    return &s_config;
}

/*---------------------------------------------------------------------------*
  Name:         PADGetKeyboardBinding

  Description:  Gets the keyboard scancode for a specific GC button.

  Arguments:    button    GC button (PAD_BUTTON_* or PAD_TRIGGER_*)

  Returns:      SDL_Scancode for that button
 *---------------------------------------------------------------------------*/
SDL_Scancode PADGetKeyboardBinding(u16 button) {
    const PADConfig* cfg = PADGetConfig();
    
    switch (button) {
        case PAD_BUTTON_UP:    return cfg->kb_up;
        case PAD_BUTTON_DOWN:  return cfg->kb_down;
        case PAD_BUTTON_LEFT:  return cfg->kb_left;
        case PAD_BUTTON_RIGHT: return cfg->kb_right;
        case PAD_BUTTON_A:     return cfg->kb_a;
        case PAD_BUTTON_B:     return cfg->kb_b;
        case PAD_BUTTON_X:     return cfg->kb_x;
        case PAD_BUTTON_Y:     return cfg->kb_y;
        case PAD_BUTTON_START: return cfg->kb_start;
        case PAD_TRIGGER_L:    return cfg->kb_l;
        case PAD_TRIGGER_R:    return cfg->kb_r;
        case PAD_TRIGGER_Z:    return cfg->kb_z;
        default: return SDL_SCANCODE_UNKNOWN;
    }
}

/*---------------------------------------------------------------------------*
  Name:         PADGetGamepadMapping

  Description:  Gets the SDL button mapping for a specific GC button.
                Returns -1 if using default mapping.

  Arguments:    button    GC button (PAD_BUTTON_* or PAD_TRIGGER_*)

  Returns:      SDL_GameControllerButton or -1 for default
 *---------------------------------------------------------------------------*/
int PADGetGamepadMapping(u16 button) {
    const PADConfig* cfg = PADGetConfig();
    
    switch (button) {
        case PAD_BUTTON_A:     return cfg->gp_a;
        case PAD_BUTTON_B:     return cfg->gp_b;
        case PAD_BUTTON_X:     return cfg->gp_x;
        case PAD_BUTTON_Y:     return cfg->gp_y;
        case PAD_BUTTON_START: return cfg->gp_start;
        case PAD_TRIGGER_L:    return cfg->gp_l;
        case PAD_TRIGGER_R:    return cfg->gp_r;
        case PAD_TRIGGER_Z:    return cfg->gp_z;
        default: return -1;
    }
}

/*---------------------------------------------------------------------------*
  Name:         PADGetDeadzone

  Description:  Gets the configured dead zone for stick or trigger.

  Arguments:    type    0=main stick, 1=C-stick, 2=triggers

  Returns:      Dead zone value (0-127)
 *---------------------------------------------------------------------------*/
int PADGetDeadzone(int type) {
    const PADConfig* cfg = PADGetConfig();
    
    switch (type) {
        case 0: return cfg->stick_deadzone;
        case 1: return cfg->c_stick_deadzone;
        case 2: return cfg->trigger_deadzone;
        default: return 15;
    }
}

/*---------------------------------------------------------------------------*
  Name:         PADGetSensitivity

  Description:  Gets the configured sensitivity multiplier for analog stick.

  Arguments:    is_c_stick    TRUE for C-stick, FALSE for main stick

  Returns:      Sensitivity multiplier (0.1 to 2.0)
 *---------------------------------------------------------------------------*/
float PADGetSensitivity(BOOL is_c_stick) {
    const PADConfig* cfg = PADGetConfig();
    return is_c_stick ? cfg->c_stick_sensitivity : cfg->stick_sensitivity;
}

/*---------------------------------------------------------------------------*
  Name:         PADGetRumbleIntensity

  Description:  Gets the configured rumble intensity.

  Arguments:    None

  Returns:      Rumble intensity (0.0 to 1.0)
 *---------------------------------------------------------------------------*/
float PADGetRumbleIntensity(void) {
    const PADConfig* cfg = PADGetConfig();
    return cfg->rumble_intensity;
}

