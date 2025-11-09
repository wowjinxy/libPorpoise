/*---------------------------------------------------------------------------*
  PADClamp.c - Analog Stick and Trigger Clamping Utilities
  
  OVERVIEW
  ========
  
  GameCube controllers have imperfect analog sticks and triggers. Without
  clamping, these imperfections cause problems:
  
  1. Dead Zone Issues:
     - Worn sticks don't return perfectly to center (stick at rest != 0,0)
     - Triggers have play before they start registering
     - Solution: Define minimum threshold below which input = 0
  
  2. Maximum Range Issues:
     - Physical stops prevent stick from reaching theoretical maximums
     - Octagonal gate limits diagonal movement
     - Solution: Clamp to actual physical range
  
  3. Octagonal vs Circular:
     - GameCube stick has octagonal gate (physical shape)
     - Diagonal maximums are less than cardinal maximums
     - Solution: Clamp to octagon shape OR convert to circle
  
  CLAMP REGIONS
  =============
  
  Three preset clamp regions defined based on Nintendo's controller specs:
  
  ClampRegion (Original GameCube):
  - Stick dead zone: 15 units
  - Stick max (cardinal): 72 units  
  - Stick max (diagonal): 40 units
  - Trigger dead zone: 30 units
  - Trigger max: 180 units
  - Circular radius: stick=56, C-stick=44
  
  ClampRegion2 (Wider tolerance for Wii):
  - Stick dead zone: 15 units
  - Stick max (cardinal): 72 units
  - Stick max (diagonal): 47 units (more forgiving)
  - Trigger dead zone: 30 units
  - Trigger max: 180 units
  - Circular radius: stick=62, C-stick=50
  
  ClampRegion2Ex (No dead zone):
  - Stick dead zone: 0 units (no dead zone)
  - Stick max (cardinal): 87 units
  - Stick max (diagonal): 62 units
  - Trigger dead zone: 0 units
  - Trigger max: 180 units
  - Circular radius: stick=80, C-stick=68
  
  FUNCTIONS
  =========
  
  PADClamp() - Apply octagonal clamping with original GameCube parameters
  PADClampCircle() - Apply circular clamping (converts octagon to circle)
  PADClamp2() - Apply octagonal clamping with selectable parameters
  PADClampCircle2() - Apply circular clamping with selectable parameters
  PADClampTrigger() - Apply only trigger clamping with selectable parameters
  
  USAGE
  =====
  
  Call after PADRead() to clean up analog values:
  
    PADStatus pads[PAD_MAX_CONTROLLERS];
    PADRead(pads);
    PADClamp(pads);  // Apply standard clamping
    
  Or use circular clamping for smoother 360° movement:
  
    PADClampCircle(pads);
    
  Or use custom parameters:
  
    PADClamp2(pads, PAD_STICK_CLAMP_OCTA_NO_MARGIN);
 *---------------------------------------------------------------------------*/

#include <dolphin/pad.h>
#include <math.h>
#include <stdlib.h>

/*---------------------------------------------------------------------------*
    Clamp Region Parameters
 *---------------------------------------------------------------------------*/

typedef struct PADClampRegion {
    u8  minTrigger;     ///< Trigger dead zone (values below this = 0)
    u8  maxTrigger;     ///< Trigger maximum (values above clamped to this)
    s8  minStick;       ///< Stick dead zone (distance from center to ignore)
    s8  maxStick;       ///< Stick maximum on cardinal axis (X or Y alone)
    s8  xyStick;        ///< Stick maximum on diagonal (X and Y together)
    s8  minSubstick;    ///< C-stick dead zone
    s8  maxSubstick;    ///< C-stick maximum on cardinal axis
    s8  xySubstick;     ///< C-stick maximum on diagonal
    s8  radStick;       ///< Stick circular radius (for circular clamping)
    s8  radSubstick;    ///< C-stick circular radius
} PADClampRegion;

// GameCube Standard Controller (original specifications)
static const PADClampRegion ClampRegion = {
    30,     // minTrigger    - Triggers start registering at 30/255
    180,    // maxTrigger    - Triggers max out at 180/255
    15,     // minStick      - Stick dead zone radius
    72,     // maxStick      - Max stick value on cardinal axis
    40,     // xyStick       - Max stick value on 45° diagonal
    15,     // minSubstick   - C-stick dead zone radius
    59,     // maxSubstick   - Max C-stick value on cardinal axis
    31,     // xySubstick    - Max C-stick value on 45° diagonal
    56,     // radStick      - Circular radius for stick
    44,     // radSubstick   - Circular radius for C-stick
};

// Wider parameters for more forgiving input (Wii recommended values)
static const PADClampRegion ClampRegion2 = {
    30,     // minTrigger
    180,    // maxTrigger
    15,     // minStick
    72,     // maxStick
    47,     // xyStick      - More forgiving diagonal
    15,     // minSubstick
    59,     // maxSubstick
    37,     // xySubstick   - More forgiving diagonal
    62,     // radStick     - Larger circular radius
    50,     // radSubstick  - Larger circular radius
};

// Extended parameters with no dead zone (raw input)
static const PADClampRegion ClampRegion2Ex = {
    0,      // minTrigger   - No dead zone
    180,    // maxTrigger
    0,      // minStick     - No dead zone
    87,     // maxStick     - Wider range
    62,     // xyStick
    0,      // minSubstick  - No dead zone
    74,     // maxSubstick  - Wider range
    52,     // xySubstick
    80,     // radStick     - Larger circular radius
    68,     // radSubstick  - Larger circular radius
};

/*---------------------------------------------------------------------------*
  Name:         ClampStick

  Description:  Clamp analog stick to octagonal region matching GameCube
                controller's physical gate. Applies dead zone and clamps
                to maximum values.
                
                The octagonal shape means:
                - Cardinal directions (pure X or Y) can reach 'max'
                - Diagonal directions (X and Y together) reach only 'xy'
                - This matches the physical octagonal gate on real controllers

  Arguments:    px      Pointer to X axis value (modified in place)
                py      Pointer to Y axis value (modified in place)
                max     Maximum value on cardinal axis
                xy      Maximum value on diagonal
                min     Dead zone threshold

  Returns:      None (modifies *px and *py)
 *---------------------------------------------------------------------------*/
static void ClampStick(s8* px, s8* py, s8 max, s8 xy, s8 min) {
    int x = *px;
    int y = *py;
    int signX;
    int signY;
    int d;
    
    // Extract sign (direction)
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
    
    // Apply dead zone (values below 'min' become 0)
    if (x <= min) {
        x = 0;
    } else {
        x -= min;  // Shift range down to start at 0
    }
    
    if (y <= min) {
        y = 0;
    } else {
        y -= min;
    }
    
    // If both axes in dead zone, return (0, 0)
    if (x == 0 && y == 0) {
        *px = *py = 0;
        return;
    }
    
    // Clamp to octagonal boundary
    // The octagon has 8 faces - we handle 2 cases for each quadrant:
    // 1. More horizontal (xy * y <= xy * x): clamp based on X
    // 2. More vertical (xy * x < xy * y): clamp based on Y
    
    if (xy * y <= xy * x) {
        // More horizontal - clamp using X-dominant formula
        // d = how far outside octagon boundary we are
        d = xy * x + (max - xy) * y;
        if (xy * max < d) {
            // Outside boundary - scale back to octagon edge
            x = (s8)(xy * max * x / d);
            y = (s8)(xy * max * y / d);
        }
    } else {
        // More vertical - clamp using Y-dominant formula
        d = xy * y + (max - xy) * x;
        if (xy * max < d) {
            // Outside boundary - scale back to octagon edge
            x = (s8)(xy * max * x / d);
            y = (s8)(xy * max * y / d);
        }
    }
    
    // Restore sign and write back
    *px = (s8)(signX * x);
    *py = (s8)(signY * y);
}

/*---------------------------------------------------------------------------*
  Name:         ClampCircle

  Description:  Clamp analog stick to circular region. This provides smoother
                360° movement compared to octagonal clamping, at the cost of
                not matching the physical controller gate exactly.
                
                Useful for games that need smooth circular movement (aiming,
                camera control, racing games).

  Arguments:    px        Pointer to X axis value (modified in place)
                py        Pointer to Y axis value (modified in place)
                radius    Maximum distance from center
                min       Dead zone threshold

  Returns:      None (modifies *px and *py)
 *---------------------------------------------------------------------------*/
static void ClampCircle(s8* px, s8* py, s8 radius, s8 min) {
    int x = *px;
    int y = *py;
    int squared;
    int length;
    
    // Remove vertical dead zone (symmetric around center)
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
    
    // Calculate distance from center (Pythagorean theorem)
    squared = x * x + y * y;
    
    // If outside circular radius, clamp to circle edge
    if (radius * radius < squared) {
        // Too far from center - scale back to radius
        length = (int)sqrtf((float)squared);
        x = (x * radius) / length;
        y = (y * radius) / length;
    }
    
    *px = (s8)x;
    *py = (s8)y;
}

/*---------------------------------------------------------------------------*
  Name:         ClampTrigger

  Description:  Clamp trigger value to valid range. Applies dead zone at
                bottom (trigger play before it registers) and maximum at top.

  Arguments:    trigger   Pointer to trigger value (modified in place)
                min       Minimum threshold (dead zone)
                max       Maximum value

  Returns:      None (modifies *trigger)
 *---------------------------------------------------------------------------*/
static void ClampTrigger(u8* trigger, u8 min, u8 max) {
    // Apply dead zone
    if (*trigger <= min) {
        *trigger = 0;
    } else {
        // Clamp to maximum
        if (*trigger > max) {
            *trigger = max;
        }
        // Subtract baseline (shift range to start at 0)
        *trigger -= min;
    }
}

/*---------------------------------------------------------------------------*
  Name:         PADClamp

  Description:  Clamp all controllers using standard GameCube parameters.
                Applies octagonal clamping to sticks and dead zone/max to
                triggers. This is the most common clamping function.
                
                Use this for most games to match original GameCube feel.

  Arguments:    status    Array of PAD_MAX_CONTROLLERS PADStatus structures

  Returns:      None (modifies status array in place)
 *---------------------------------------------------------------------------*/
void PADClamp(PADStatus* status) {
    if (!status) {
        return;
    }
    
    for (int i = 0; i < PAD_MAX_CONTROLLERS; i++, status++) {
        // Skip if error (controller not connected or not ready)
        if (status->err != PAD_ERR_NONE) {
            continue;
        }
        
        // Clamp main analog stick to octagon
        ClampStick(&status->stickX, &status->stickY,
                   ClampRegion.maxStick,
                   ClampRegion.xyStick,
                   ClampRegion.minStick);
        
        // Clamp C-stick to octagon
        ClampStick(&status->substickX, &status->substickY,
                   ClampRegion.maxSubstick,
                   ClampRegion.xySubstick,
                   ClampRegion.minSubstick);
        
        // Clamp triggers
        ClampTrigger(&status->triggerLeft,
                     ClampRegion.minTrigger,
                     ClampRegion.maxTrigger);
        
        ClampTrigger(&status->triggerRight,
                     ClampRegion.minTrigger,
                     ClampRegion.maxTrigger);
    }
}

/*---------------------------------------------------------------------------*
  Name:         PADClampCircle

  Description:  Clamp all controllers using circular clamping for sticks.
                Provides smoother 360° movement compared to octagonal clamping.
                
                Use this for games with circular movement (camera, aiming).

  Arguments:    status    Array of PAD_MAX_CONTROLLERS PADStatus structures

  Returns:      None (modifies status array in place)
 *---------------------------------------------------------------------------*/
void PADClampCircle(PADStatus* status) {
    if (!status) {
        return;
    }
    
    for (int i = 0; i < PAD_MAX_CONTROLLERS; i++, status++) {
        if (status->err != PAD_ERR_NONE) {
            continue;
        }
        
        // Clamp main analog stick to circle
        ClampCircle(&status->stickX, &status->stickY,
                    ClampRegion.radStick,
                    ClampRegion.minStick);
        
        // Clamp C-stick to circle
        ClampCircle(&status->substickX, &status->substickY,
                    ClampRegion.radSubstick,
                    ClampRegion.minSubstick);
        
        // Clamp triggers (same as octagonal)
        ClampTrigger(&status->triggerLeft,
                     ClampRegion.minTrigger,
                     ClampRegion.maxTrigger);
        
        ClampTrigger(&status->triggerRight,
                     ClampRegion.minTrigger,
                     ClampRegion.maxTrigger);
    }
}

/*---------------------------------------------------------------------------*
  Name:         PADClamp2

  Description:  Clamp controllers using selectable clamp parameters.
                Allows choosing between different dead zone/range settings.

  Arguments:    status    Array of PAD_MAX_CONTROLLERS PADStatus structures
                type      Clamp type:
                          PAD_STICK_CLAMP_OCTA_WITH_MARGIN - Wider tolerance
                          PAD_STICK_CLAMP_OCTA_NO_MARGIN - No dead zone

  Returns:      None (modifies status array in place)
 *---------------------------------------------------------------------------*/
void PADClamp2(PADStatus* status, u32 type) {
    if (!status) {
        return;
    }
    
    const PADClampRegion* stkreg;
    
    // Select clamp region based on type
    if (type == PAD_STICK_CLAMP_OCTA_WITH_MARGIN) {
        stkreg = &ClampRegion2;      // Wider tolerance (Wii recommended)
    } else {
        stkreg = &ClampRegion2Ex;    // No dead zone (raw input)
    }
    
    for (int i = 0; i < PAD_MAX_CONTROLLERS; i++, status++) {
        if (status->err != PAD_ERR_NONE) {
            continue;
        }
        
        // Clamp sticks only (triggers not included in this function)
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

/*---------------------------------------------------------------------------*
  Name:         PADClampCircle2

  Description:  Clamp controllers using circular clamping with selectable
                parameters. Combines circular clamping with custom dead zones.

  Arguments:    status    Array of PAD_MAX_CONTROLLERS PADStatus structures
                type      Clamp type:
                          PAD_STICK_CLAMP_CIRCLE_WITH_MARGIN - Wider tolerance
                          PAD_STICK_CLAMP_CIRCLE_NO_MARGIN - No dead zone

  Returns:      None (modifies status array in place)
 *---------------------------------------------------------------------------*/
void PADClampCircle2(PADStatus* status, u32 type) {
    if (!status) {
        return;
    }
    
    const PADClampRegion* stkreg;
    
    // Select clamp region based on type
    if (type == PAD_STICK_CLAMP_CIRCLE_WITH_MARGIN) {
        stkreg = &ClampRegion2;      // Wider tolerance
    } else {
        stkreg = &ClampRegion2Ex;    // No dead zone
    }
    
    for (int i = 0; i < PAD_MAX_CONTROLLERS; i++, status++) {
        if (status->err != PAD_ERR_NONE) {
            continue;
        }
        
        // Clamp sticks to circle
        ClampCircle(&status->stickX, &status->stickY,
                    stkreg->radStick,
                    stkreg->minStick);
        
        ClampCircle(&status->substickX, &status->substickY,
                    stkreg->radSubstick,
                    stkreg->minSubstick);
    }
}

/*---------------------------------------------------------------------------*
  Name:         PADClampTrigger

  Description:  Clamp only trigger values with selectable parameters.
                Useful when you want to clamp triggers separately from sticks.

  Arguments:    status    Array of PAD_MAX_CONTROLLERS PADStatus structures
                type      Clamp type:
                          PAD_TRIGGER_FIXED_BASE - Standard dead zone
                          PAD_TRIGGER_OPEN_BASE - No dead zone

  Returns:      None (modifies status array in place)
 *---------------------------------------------------------------------------*/
void PADClampTrigger(PADStatus* status, u32 type) {
    if (!status) {
        return;
    }
    
    const PADClampRegion* stkreg;
    
    // Select clamp region based on type
    if (type == PAD_TRIGGER_FIXED_BASE) {
        stkreg = &ClampRegion2;      // Standard dead zone
    } else {
        stkreg = &ClampRegion2Ex;    // No dead zone (raw input)
    }
    
    for (int i = 0; i < PAD_MAX_CONTROLLERS; i++, status++) {
        if (status->err != PAD_ERR_NONE) {
            continue;
        }
        
        // Clamp both triggers
        ClampTrigger(&status->triggerLeft,
                     stkreg->minTrigger,
                     stkreg->maxTrigger);
        
        ClampTrigger(&status->triggerRight,
                     stkreg->minTrigger,
                     stkreg->maxTrigger);
    }
}
