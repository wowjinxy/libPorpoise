/*---------------------------------------------------------------------------*
  Reset Button Example
  
  Demonstrates how to handle reset button on PC using OSResetSW API.
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <stdio.h>
#include <stdlib.h>

static BOOL g_running = TRUE;

/*---------------------------------------------------------------------------*
  Reset Callback Handler
 *---------------------------------------------------------------------------*/
void OnResetButton(void) {
    OSReport("\n");
    OSReport("========================================\n");
    OSReport("RESET BUTTON PRESSED!\n");
    OSReport("========================================\n");
    OSReport("Saving game state...\n");
    
    /* Simulate saving game */
    OSReport("Game saved successfully!\n");
    
    /* Ask user what to do */
    OSReport("\n");
    OSReport("What would you like to do?\n");
    OSReport("1. Return to Menu\n");
    OSReport("2. Restart Game\n");
    OSReport("3. Continue Playing\n");
    OSReport("\n");
    
    /* For this example, we'll just quit */
    OSReport("Returning to menu...\n");
    g_running = FALSE;
}

/*---------------------------------------------------------------------------*
  Power Callback Handler
 *---------------------------------------------------------------------------*/
void OnPowerButton(void) {
    OSReport("\n");
    OSReport("========================================\n");
    OSReport("POWER BUTTON PRESSED!\n");
    OSReport("========================================\n");
    OSReport("Shutting down gracefully...\n");
    
    g_running = FALSE;
}

/*---------------------------------------------------------------------------*
  Main Function
 *---------------------------------------------------------------------------*/
int main(void) {
    /* Initialize OS */
    OSInit();
    
    OSReport("\n");
    OSReport("========================================\n");
    OSReport("Reset Button Example\n");
    OSReport("========================================\n");
    OSReport("\n");
    
    /* Register callbacks */
    OSSetResetCallback(OnResetButton);
    OSSetPowerCallback(OnPowerButton);
    
    OSReport("Reset and power callbacks registered!\n");
    OSReport("\n");
    OSReport("On PC, you need to manually trigger these:\n");
    OSReport("- Press ESC or 'R' in your game to call OSSimulateResetButton()\n");
    OSReport("- Press 'P' or window close to call OSSimulatePowerButton()\n");
    OSReport("\n");
    
    /* Simulate a game loop */
    OSReport("Game running... (simulating button presses)\n");
    OSReport("\n");
    
    int frame = 0;
    while (g_running && frame < 500) {
        /* Simulate game frame */
        frame++;
        
        /* Poll reset button (will always be FALSE unless simulated) */
        if (OSGetResetButtonState()) {
            OSReport("Reset button detected via polling!\n");
            OnResetButton();
            break;
        }
        
        /* Simulate button press after 100 frames */
        if (frame == 100) {
            OSReport("\n--- Simulating RESET button press ---\n");
            OSSimulateResetButton();
            /* This calls OnResetButton() automatically */
        }
        
        /* Simulate power button after 200 frames */
        if (frame == 200) {
            OSReport("\n--- Simulating POWER button press ---\n");
            OSSimulatePowerButton();
            /* This calls OnPowerButton() automatically */
        }
        
        /* Small delay */
        OSSleepMilliseconds(16);  /* ~60 FPS */
    }
    
    OSReport("\n");
    OSReport("Example complete!\n");
    OSReport("\n");
    OSReport("INTEGRATION GUIDE:\n");
    OSReport("==================\n");
    OSReport("\n");
    OSReport("In your actual game, hook up input:\n");
    OSReport("\n");
    OSReport("  // SDL example:\n");
    OSReport("  if (event.type == SDL_QUIT) {\n");
    OSReport("      OSSimulatePowerButton();\n");
    OSReport("  }\n");
    OSReport("  if (event.key.keysym.sym == SDLK_ESCAPE) {\n");
    OSReport("      OSSimulateResetButton();\n");
    OSReport("  }\n");
    OSReport("\n");
    OSReport("  // GLFW example:\n");
    OSReport("  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {\n");
    OSReport("      OSSimulateResetButton();\n");
    OSReport("  }\n");
    OSReport("\n");
    
    return 0;
}

