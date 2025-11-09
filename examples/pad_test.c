/**
 * @file pad_test.c
 * @brief Example program demonstrating controller input with libPorpoise
 * 
 * Shows how to initialize controllers, read input, and handle rumble.
 * Supports up to 4 controllers with keyboard fallback.
 */

#include <dolphin/os.h>
#include <dolphin/pad.h>
#include <stdio.h>
#include <stdlib.h>

// Pretty print button names
static void PrintButtons(u16 buttons) {
    if (buttons & PAD_BUTTON_A)      printf("A ");
    if (buttons & PAD_BUTTON_B)      printf("B ");
    if (buttons & PAD_BUTTON_X)      printf("X ");
    if (buttons & PAD_BUTTON_Y)      printf("Y ");
    if (buttons & PAD_BUTTON_START)  printf("START ");
    if (buttons & PAD_BUTTON_LEFT)   printf("LEFT ");
    if (buttons & PAD_BUTTON_RIGHT)  printf("RIGHT ");
    if (buttons & PAD_BUTTON_UP)     printf("UP ");
    if (buttons & PAD_BUTTON_DOWN)   printf("DOWN ");
    if (buttons & PAD_TRIGGER_L)     printf("L ");
    if (buttons & PAD_TRIGGER_R)     printf("R ");
    if (buttons & PAD_TRIGGER_Z)     printf("Z ");
}

int main(void) {
    PADStatus pads[PAD_MAX_CONTROLLERS];
    BOOL running = TRUE;
    int frame = 0;
    
    // Initialize OS
    OSReport("Initializing libPorpoise...\n");
    OSInit();
    
    // Initialize PAD
    OSReport("\nInitializing controller subsystem...\n");
    if (!PADInit()) {
        OSReport("Failed to initialize PAD!\n");
        return 1;
    }
    
    // Wait for PAD to be ready
    OSReport("Waiting for controllers...\n");
    while (!PADSync()) {
        OSSleepTicks(OSMillisecondsToTicks(16));
    }
    
    OSReport("\n==============================================\n");
    OSReport("   libPorpoise Controller Test\n");
    OSReport("==============================================\n");
    OSReport("Press START on any controller to exit\n");
    OSReport("\nKeyboard controls (Player 1):\n");
    OSReport("  Arrow keys     - D-pad / Stick\n");
    OSReport("  Z, X, C, V     - A, B, X, Y\n");
    OSReport("  A, S, D        - L, R, Z triggers\n");
    OSReport("  I, K, J, L     - C-stick\n");
    OSReport("  Enter          - START\n");
    OSReport("==============================================\n\n");
    
    // Main loop
    while (running) {
        // Read all controllers
        u32 motorMask = PADRead(pads);
        
        // Check each controller
        for (int i = 0; i < PAD_MAX_CONTROLLERS; i++) {
            PADStatus* pad = &pads[i];
            
            // Skip if not connected or error
            if (pad->err != PAD_ERR_NONE) {
                continue;
            }
            
            // Check for START to exit
            if (pad->button & PAD_BUTTON_START) {
                OSReport("\nSTART pressed - exiting...\n");
                running = FALSE;
                break;
            }
            
            // Only print status every 60 frames if input changed
            static u16 lastButtons[PAD_MAX_CONTROLLERS] = {0};
            static s8 lastStickX[PAD_MAX_CONTROLLERS] = {0};
            static s8 lastStickY[PAD_MAX_CONTROLLERS] = {0};
            
            BOOL changed = (pad->button != lastButtons[i]) ||
                          (abs(pad->stickX - lastStickX[i]) > 10) ||
                          (abs(pad->stickY - lastStickY[i]) > 10);
            
            if (changed) {
                OSReport("[P%d] Buttons: ", i + 1);
                if (pad->button == 0) {
                    printf("(none) ");
                } else {
                    PrintButtons(pad->button);
                }
                
                printf("| Stick: (%4d, %4d) ", pad->stickX, pad->stickY);
                printf("| C: (%4d, %4d) ", pad->substickX, pad->substickY);
                printf("| L/R: (%3d, %3d)\n", pad->triggerLeft, pad->triggerRight);
                
                lastButtons[i] = pad->button;
                lastStickX[i] = pad->stickX;
                lastStickY[i] = pad->stickY;
                
                // Test rumble on A button
                if (pad->button & PAD_BUTTON_A) {
                    if (motorMask & (PAD_CHAN0_BIT >> i)) {
                        PADControlMotor(i, PAD_MOTOR_RUMBLE);
                        OSReport("   -> Rumble activated!\n");
                    }
                } else {
                    PADControlMotor(i, PAD_MOTOR_STOP);
                }
            }
        }
        
        // Sleep for ~16ms (60 FPS)
        OSSleepTicks(OSMillisecondsToTicks(16));
        frame++;
    }
    
    // Stop all rumble
    u32 stopCommands[PAD_MAX_CONTROLLERS] = {
        PAD_MOTOR_STOP, PAD_MOTOR_STOP, PAD_MOTOR_STOP, PAD_MOTOR_STOP
    };
    PADControlAllMotors(stopCommands);
    
    OSReport("\nController test completed!\n");
    OSReport("Total frames: %d\n", frame);
    
    return 0;
}

