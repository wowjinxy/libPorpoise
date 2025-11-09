/*---------------------------------------------------------------------------*
  OSResetSW.c - Reset Button Handling
  
  HARDWARE RESET BUTTON (GC/Wii):
  ================================
  
  **Physical Reset Button:**
  - GameCube/Wii have a physical RESET button on the console
  - Button state read from PI_INTSR register (PI_INTSR_REG_RSTVAL_MASK)
  - Generates PI_RSW interrupt when pressed
  - Used for soft reset during gameplay
  
  **How It Works:**
  
  1. **Interrupt-Driven (Default):**
     ```c
     OSSetResetCallback(MyResetHandler);
     
     void MyResetHandler() {
         SaveGame();
         OSRestart(0);  // Soft reset
     }
     ```
     - Reset button press triggers interrupt
     - Calls registered callback
     - Game decides what to do (save, restart, return to menu, etc.)
  
  2. **Polling Mode:**
     ```c
     while (running) {
         if (OSGetResetButtonState()) {
             // User pressed reset button
             HandleReset();
         }
         GameLoop();
     }
     ```
  
  **Debouncing:**
  - Hardware button can bounce (multiple transitions)
  - OSResetSW implements debouncing:
    * Noise filter: 100 microseconds
    * Hold-up time: 40 milliseconds
  - Prevents accidental multiple resets
  
  **Power Button (Wii):**
  - Wii also has POWER button
  - Similar to reset but for shutdown
  - OSSetPowerCallback() registers handler
  
  **Special Feature: Game Choice Timer**
  - IPL uses this for boot menu timeout
  - __OSSetResetButtonTimer(minutes)
  - After timeout, reset button state oscillates
  - Not commonly used in games
  
  PC PORT STRATEGY:
  =================
  
  **Challenge:** No physical reset button on PC!
  
  **Option 1: No-Op (Current)** ✅
  - OSGetResetButtonState() always returns FALSE
  - Callbacks can be registered but won't be called automatically
  - Games that poll will never see reset pressed
  - Games must provide their own quit mechanism (ESC key, etc.)
  
  **Option 2: Keyboard Simulation** 🎮
  - Map a key to reset button (e.g., ESC, F12)
  - Set global flag when key pressed
  - OSGetResetButtonState() returns flag
  - Requires keyboard input integration
  
  **Option 3: Signal Handler** 🔧
  - Use Ctrl+C (SIGINT) as reset signal
  - Signal handler sets flag
  - OSGetResetButtonState() returns flag
  - Works in terminal but not in fullscreen games
  
  **Option 4: External API** 🎯
  - Provide OSSimulateResetButton() function
  - Game/engine calls it when user presses quit
  - Triggers callbacks
  - Most flexible approach
  
  CURRENT IMPLEMENTATION:
  =======================
  
  We use **Option 1 (No-Op) + Option 4 (External API)**:
  - Normal operation: Returns FALSE (no hardware button)
  - OSSimulateResetButton(): Manually trigger callbacks
  - Games can call this when user presses ESC or quit button
  
  MIGRATION FOR GAMES:
  ====================
  
  **If your game uses reset button:**
  
  1. **Interrupt-driven callback:**
     ```c
     // Old (automatic):
     OSSetResetCallback(OnReset);
     
     // New (manual trigger):
     OSSetResetCallback(OnReset);
     if (InputIsKeyPressed(KEY_ESCAPE)) {
         OSSimulateResetButton();  // Calls OnReset()
     }
     ```
  
  2. **Polling:**
     ```c
     // Old:
     if (OSGetResetButtonState()) { ... }
     
     // New:
     if (InputIsKeyPressed(KEY_ESCAPE)) { ... }
     ```
  
  Most games should just add ESC key handling or window close events.
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <string.h>

/* Callbacks */
static OSResetCallback s_resetCallback = NULL;
static OSPowerCallback s_powerCallback = NULL;

/* Button state simulation */
static BOOL s_resetButtonPressed = FALSE;
static BOOL s_powerButtonPressed = FALSE;

/* Debouncing state */
static BOOL   s_buttonDown = FALSE;
static BOOL   s_lastState = FALSE;
static OSTime s_holdUp = 0;
static OSTime s_holdDown = 0;

/* Debouncing delays (from reference) */
#define DEBOUNCING_DELAY  40    /* milliseconds */
#define NOISE_DELAY       100   /* microseconds */

/*---------------------------------------------------------------------------*
  Name:         OSGetResetButtonState

  Description:  Checks if the reset button is currently pressed.
                
                On original hardware:
                - Reads PI_INTSR register (PI_INTSR_REG_RSTVAL_MASK)
                - Implements debouncing (40ms hold-up, 100us noise filter)
                - Returns TRUE if button held down
                
                On PC:
                - No hardware button
                - Returns simulated state (set via OSSimulateResetButton)
                - Always FALSE unless manually triggered

  Returns:      TRUE if reset button pressed, FALSE otherwise
  
  Thread Safety: Safe to call from any thread
 *---------------------------------------------------------------------------*/
BOOL OSGetResetButtonState(void) {
    BOOL enabled;
    BOOL state;
    OSTime now;
    
    enabled = OSDisableInterrupts();
    
    /* On PC, we simulate button state */
    state = s_resetButtonPressed;
    
    /* Debouncing simulation */
    now = OSGetSystemTime();
    
    if (s_resetButtonPressed) {
        /* Button is "pressed" */
        if (!s_buttonDown) {
            s_buttonDown = TRUE;
            state = s_holdUp ? TRUE : FALSE;
            s_holdDown = now;
        } else {
            /* Check if held long enough to be real (noise filter) */
            state = (s_holdUp || (OSMicrosecondsToTicks(NOISE_DELAY) < now - s_holdDown)) ? TRUE : FALSE;
        }
    } else {
        /* Button is "released" */
        if (s_buttonDown) {
            s_buttonDown = FALSE;
            state = s_lastState;
            if (state) {
                s_holdUp = now;
            } else {
                s_holdUp = 0;
            }
        } else if (s_holdUp && (now - s_holdUp < OSMillisecondsToTicks(DEBOUNCING_DELAY))) {
            /* Still within debounce window */
            state = TRUE;
        } else {
            state = FALSE;
            s_holdUp = 0;
        }
    }
    
    s_lastState = state;
    
    OSRestoreInterrupts(enabled);
    
    /* Clear simulated press after reading */
    s_resetButtonPressed = FALSE;
    
    return state;
}

/*---------------------------------------------------------------------------*
  Name:         OSGetResetSwitchState

  Description:  Alias for OSGetResetButtonState() (old SDK compatibility).

  Returns:      TRUE if reset button pressed, FALSE otherwise
 *---------------------------------------------------------------------------*/
BOOL OSGetResetSwitchState(void) {
    return OSGetResetButtonState();
}

/*---------------------------------------------------------------------------*
  Name:         OSSetResetCallback

  Description:  Registers a callback to be called when reset button is pressed.
                
                On original hardware:
                - Installs interrupt handler for PI_RSW
                - Unmasks reset switch interrupt
                - Callback is one-shot (cleared after calling)
                
                On PC:
                - Stores callback
                - Called when OSSimulateResetButton() is invoked
                - Games must manually trigger

  Arguments:    callback - Function to call (NULL to disable)

  Returns:      Previous callback
  
  Example:
    void OnReset() {
        OSReport("Reset button pressed!\n");
        SaveGame();
        OSReturnToMenu();
    }
    
    OSSetResetCallback(OnReset);
    
    // Later, when user presses ESC or close button:
    OSSimulateResetButton();  // Calls OnReset()
 *---------------------------------------------------------------------------*/
OSResetCallback OSSetResetCallback(OSResetCallback callback) {
    BOOL enabled = OSDisableInterrupts();
    
    OSResetCallback prev = s_resetCallback;
    s_resetCallback = callback;
    
#ifdef _DEBUG
    if (callback) {
        OSReport("[OSResetSW] Reset callback registered\n");
        OSReport("            Call OSSimulateResetButton() to trigger on PC\n");
    } else {
        OSReport("[OSResetSW] Reset callback cleared\n");
    }
#endif
    
    OSRestoreInterrupts(enabled);
    return prev;
}

/*---------------------------------------------------------------------------*
  Name:         OSSetPowerCallback

  Description:  Registers a callback for power button (Wii).
                
                On original hardware:
                - Wii power button triggers callback
                - Used for graceful shutdown
                
                On PC:
                - Stores callback
                - Called when OSSimulatePowerButton() is invoked

  Arguments:    callback - Function to call (NULL to disable)

  Returns:      Previous callback
 *---------------------------------------------------------------------------*/
OSPowerCallback OSSetPowerCallback(OSPowerCallback callback) {
    BOOL enabled = OSDisableInterrupts();
    
    OSPowerCallback prev = s_powerCallback;
    s_powerCallback = callback;
    
#ifdef _DEBUG
    if (callback) {
        OSReport("[OSResetSW] Power callback registered\n");
        OSReport("            Call OSSimulatePowerButton() to trigger on PC\n");
    } else {
        OSReport("[OSResetSW] Power callback cleared\n");
    }
#endif
    
    OSRestoreInterrupts(enabled);
    return prev;
}

/*===========================================================================*
  PC-SPECIFIC UTILITIES (Not in original SDK)
 *===========================================================================*/

/*---------------------------------------------------------------------------*
  Name:         OSSimulateResetButton (PC Extension)

  Description:  Manually triggers reset button press simulation.
                
                Call this from your input handler when user presses ESC,
                or from window close event handler.
                
                This will:
                1. Set internal reset button flag
                2. Call registered reset callback (if any)

  Arguments:    None
  
  Example:
    // In your input handling:
    if (IsKeyPressed(KEY_ESCAPE)) {
        OSSimulateResetButton();
    }
    
    // Or in window close event:
    void OnWindowClose() {
        OSSimulateResetButton();
    }
 *---------------------------------------------------------------------------*/
void OSSimulateResetButton(void) {
    BOOL enabled = OSDisableInterrupts();
    
    /* Set simulated button state */
    s_resetButtonPressed = TRUE;
    
    /* Call registered callback (one-shot) */
    if (s_resetCallback) {
        OSResetCallback callback = s_resetCallback;
        s_resetCallback = NULL;  /* One-shot, clear after calling */
        
#ifdef _DEBUG
        OSReport("[OSResetSW] Reset button simulated, calling callback\n");
#endif
        
        OSRestoreInterrupts(enabled);
        callback();  /* Call with interrupts enabled */
        return;
    }
    
    OSRestoreInterrupts(enabled);
    
#ifdef _DEBUG
    OSReport("[OSResetSW] Reset button simulated (no callback registered)\n");
#endif
}

/*---------------------------------------------------------------------------*
  Name:         OSSimulatePowerButton (PC Extension)

  Description:  Manually triggers power button press simulation.
                
                Call this when user wants to shutdown/quit.

  Arguments:    None
 *---------------------------------------------------------------------------*/
void OSSimulatePowerButton(void) {
    BOOL enabled = OSDisableInterrupts();
    
    /* Set simulated button state */
    s_powerButtonPressed = TRUE;
    
    /* Call registered callback */
    if (s_powerCallback) {
        OSPowerCallback callback = s_powerCallback;
        
#ifdef _DEBUG
        OSReport("[OSResetSW] Power button simulated, calling callback\n");
#endif
        
        OSRestoreInterrupts(enabled);
        callback();  /* Call with interrupts enabled */
        return;
    }
    
    OSRestoreInterrupts(enabled);
    
#ifdef _DEBUG
    OSReport("[OSResetSW] Power button simulated (no callback registered)\n");
#endif
}

/*---------------------------------------------------------------------------*
  Name:         __OSResetSWInit (Internal)

  Description:  Initializes reset switch system.
                Called by OSInit().

  Arguments:    None
 *---------------------------------------------------------------------------*/
void __OSResetSWInit(void) {
    s_resetCallback = NULL;
    s_powerCallback = NULL;
    s_resetButtonPressed = FALSE;
    s_powerButtonPressed = FALSE;
    s_buttonDown = FALSE;
    s_lastState = FALSE;
    s_holdUp = 0;
    s_holdDown = 0;
    
#ifdef _DEBUG
    OSReport("[OSResetSW] Initialized (PC mode - use OSSimulateResetButton)\n");
#endif
}
