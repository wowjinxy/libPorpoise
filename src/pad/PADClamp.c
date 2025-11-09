/**
 * @file PADClamp.c
 * @brief Analog Stick and Trigger Clamping for libPorpoise
 * 
 * Provides dead zone handling and octagonal/circular clamping for analog inputs.
 * Based on Nintendo's original PADClamp implementation.
 * 
 * Copyright (c) 2025 libPorpoise Contributors
 * SPDX-License-Identifier: MIT
 */

#include <dolphin/pad.h>
#include <math.h>
#include <stdlib.h>

/*---------------------------------------------------------------------------*
    Clamp Region Parameters
 *---------------------------------------------------------------------------*/

typedef struct PADClampRegion {
    u8  minTrigger;     ///< Trigger dead zone
    u8  maxTrigger;     ///< Trigger maximum
    s8  minStick;       ///< Stick dead zone
    s8  maxStick;       ///< Stick maximum on axis
    s8  xyStick;        ///< Stick maximum on diagonal
    s8  minSubstick;    ///< C-stick dead zone
    s8  maxSubstick;    ///< C-stick maximum on axis
    s8  xySubstick;     ///< C-stick maximum on diagonal
    s8  radStick;       ///< Stick circular radius
    s8  radSubstick;    ///< C-stick circular radius
} PADClampRegion;

// GameCube Standard Controller parameters
static const PADClampRegion ClampRegion = {
    30,     // minTrigger
    180,    // maxTrigger
    15,     // minStick
    72,     // maxStick
    40,     // xyStick
    15,     // minSubstick
    59,     // maxSubstick
    31,     // xySubstick
    56,     // radStick
    44,     // radSubstick
};

// Wider parameters for more forgiving input
static const PADClampRegion ClampRegion2 = {
    30,     // minTrigger
    180,    // maxTrigger
    15,     // minStick
    72,     // maxStick
    47,     // xyStick
    15,     // minSubstick
    59,     // maxSubstick
    37,     // xySubstick
    62,     // radStick
    50,     // radSubstick
};

// Extended parameters with no dead zone
static const PADClampRegion ClampRegion2Ex = {
    0,      // minTrigger
    180,    // maxTrigger
    0,      // minStick
    87,     // maxStick
    62,     // xyStick
    0,      // minSubstick
    74,     // maxSubstick
    52,     // xySubstick
    80,     // radStick
    68,     // radSubstick
};

/*---------------------------------------------------------------------------*
    Internal Functions
 *---------------------------------------------------------------------------*/

/**
 * @brief Clamp stick to octagonal region
 * 
 * Applies dead zone and clamps to octagonal boundary matching
 * GameCube controller's physical range of motion.
 * 
 * @param px Pointer to X axis value
 * @param py Pointer to Y axis value
 * @param max Maximum value on cardinal axis
 * @param xy Maximum value on diagonal
 * @param min Dead zone threshold
 */
static void ClampStick(s8* px, s8* py, s8 max, s8 xy, s8 min) {
    int x = *px;
    int y = *py;
    int signX;
    int signY;
    int d;
    
    // Get signs
    if (x >= 0) {
        signX = 1;
    } else {
        signX = -1;
        x = -x;
    }
    
    if (y >= 0) {
        signY = 1;
    } else {
        signY = -1;
        y = -y;
    }
    
    // Apply dead zone
    if (x <= min) {
        x = 0;
    } else {
        x -= min;
    }
    
    if (y <= min) {
        y = 0;
    } else {
        y -= min;
    }
    
    if (x == 0 && y == 0) {
        *px = *py = 0;
        return;
    }
    
    // Clamp to octagonal boundary
    if (xy * y <= xy * x) {
        d = xy * x + (max - xy) * y;
        if (xy * max < d) {
            x = (s8)(xy * max * x / d);
            y = (s8)(xy * max * y / d);
        }
    } else {
        d = xy * y + (max - xy) * x;
        if (xy * max < d) {
            x = (s8)(xy * max * x / d);
            y = (s8)(xy * max * y / d);
        }
    }
    
    *px = (s8)(signX * x);
    *py = (s8)(signY * y);
}

/**
 * @brief Clamp stick to circular region
 * 
 * Applies dead zone and clamps to circular boundary.
 * 
 * @param px Pointer to X axis value
 * @param py Pointer to Y axis value
 * @param radius Maximum radius
 * @param min Dead zone threshold
 */
static void ClampCircle(s8* px, s8* py, s8 radius, s8 min) {
    int x = *px;
    int y = *py;
    int squared;
    int length;
    
    // Remove vertical dead zone
    if (-min < x && x < min) {
        x = 0;
    } else if (x > 0) {
        x -= min;
    } else {
        x += min;
    }
    
    // Remove horizontal dead zone
    if (-min < y && y < min) {
        y = 0;
    } else if (y > 0) {
        y -= min;
    } else {
        y += min;
    }
    
    // Clamp to circular radius
    squared = x * x + y * y;
    if (radius * radius < squared) {
        // Vector too long - clamp to radius
        length = (int)sqrtf((float)squared);
        x = (x * radius) / length;
        y = (y * radius) / length;
    }
    
    *px = (s8)x;
    *py = (s8)y;
}

/**
 * @brief Clamp trigger to valid range
 * 
 * Applies dead zone and maximum to trigger input.
 * 
 * @param trigger Pointer to trigger value
 * @param min Minimum threshold
 * @param max Maximum value
 */
static void ClampTrigger(u8* trigger, u8 min, u8 max) {
    if (*trigger <= min) {
        *trigger = 0;
    } else {
        if (*trigger > max) {
            *trigger = max;
        }
        *trigger -= min;
    }
}

/*---------------------------------------------------------------------------*
    Public API
 *---------------------------------------------------------------------------*/

void PADClamp(PADStatus* status) {
    if (!status) {
        return;
    }
    
    for (int i = 0; i < PAD_MAX_CONTROLLERS; i++, status++) {
        if (status->err != PAD_ERR_NONE) {
            continue;
        }
        
        ClampStick(&status->stickX, &status->stickY,
                   ClampRegion.maxStick,
                   ClampRegion.xyStick,
                   ClampRegion.minStick);
        
        ClampStick(&status->substickX, &status->substickY,
                   ClampRegion.maxSubstick,
                   ClampRegion.xySubstick,
                   ClampRegion.minSubstick);
        
        ClampTrigger(&status->triggerLeft,
                     ClampRegion.minTrigger,
                     ClampRegion.maxTrigger);
        
        ClampTrigger(&status->triggerRight,
                     ClampRegion.minTrigger,
                     ClampRegion.maxTrigger);
    }
}

void PADClampCircle(PADStatus* status) {
    if (!status) {
        return;
    }
    
    for (int i = 0; i < PAD_MAX_CONTROLLERS; i++, status++) {
        if (status->err != PAD_ERR_NONE) {
            continue;
        }
        
        ClampCircle(&status->stickX, &status->stickY,
                    ClampRegion.radStick,
                    ClampRegion.minStick);
        
        ClampCircle(&status->substickX, &status->substickY,
                    ClampRegion.radSubstick,
                    ClampRegion.minSubstick);
        
        ClampTrigger(&status->triggerLeft,
                     ClampRegion.minTrigger,
                     ClampRegion.maxTrigger);
        
        ClampTrigger(&status->triggerRight,
                     ClampRegion.minTrigger,
                     ClampRegion.maxTrigger);
    }
}

void PADClamp2(PADStatus* status, u32 type) {
    if (!status) {
        return;
    }
    
    const PADClampRegion* stkreg;
    
    if (type == PAD_STICK_CLAMP_OCTA_WITH_MARGIN) {
        stkreg = &ClampRegion2;
    } else {
        stkreg = &ClampRegion2Ex;
    }
    
    for (int i = 0; i < PAD_MAX_CONTROLLERS; i++, status++) {
        if (status->err != PAD_ERR_NONE) {
            continue;
        }
        
        ClampStick(&status->stickX, &status->stickY,
                   stkreg->maxStick,
                   stkreg->xyStick,
                   stkreg->minStick);
        
        ClampStick(&status->substickX, &status->substickY,
                   stkreg->maxSubstick,
                   stkreg->xySubstick,
                   stkreg->minSubstick);
    }
}

void PADClampCircle2(PADStatus* status, u32 type) {
    if (!status) {
        return;
    }
    
    const PADClampRegion* stkreg;
    
    if (type == PAD_STICK_CLAMP_CIRCLE_WITH_MARGIN) {
        stkreg = &ClampRegion2;
    } else {
        stkreg = &ClampRegion2Ex;
    }
    
    for (int i = 0; i < PAD_MAX_CONTROLLERS; i++, status++) {
        if (status->err != PAD_ERR_NONE) {
            continue;
        }
        
        ClampCircle(&status->stickX, &status->stickY,
                    stkreg->radStick,
                    stkreg->minStick);
        
        ClampCircle(&status->substickX, &status->substickY,
                    stkreg->radSubstick,
                    stkreg->minSubstick);
    }
}

void PADClampTrigger(PADStatus* status, u32 type) {
    if (!status) {
        return;
    }
    
    const PADClampRegion* stkreg;
    
    if (type == PAD_TRIGGER_FIXED_BASE) {
        stkreg = &ClampRegion2;
    } else {
        stkreg = &ClampRegion2Ex;
    }
    
    for (int i = 0; i < PAD_MAX_CONTROLLERS; i++, status++) {
        if (status->err != PAD_ERR_NONE) {
            continue;
        }
        
        ClampTrigger(&status->triggerLeft,
                     stkreg->minTrigger,
                     stkreg->maxTrigger);
        
        ClampTrigger(&status->triggerRight,
                     stkreg->minTrigger,
                     stkreg->maxTrigger);
    }
}

