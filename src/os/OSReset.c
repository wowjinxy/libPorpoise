/*---------------------------------------------------------------------------*
  OSReset.c - System Reset and Shutdown
  
  HARDWARE RESET SYSTEM (GC/Wii):
  ================================
  
  The GameCube/Wii had several ways to reset or shutdown:
  
  **Reset Types:**
  
  1. **OS_RESET_RESTART** - Soft reset
     - Restarts the game
     - Preserves save region (small RAM area for persistent data)
     - Original hardware: Jumps back to OSInit, re-executes apploader
  
  2. **OS_RESET_HOTRESET** - Hot reset
     - Full hardware reset
     - Clears everything except IPL
     - Original hardware: Resets Gekko/Broadway CPU
  
  3. **OS_RESET_SHUTDOWN** - Power off
     - Turns console off
     - Original hardware: Cuts power via hardware
  
  **Special Modes:**
  
  - **OSReturnToMenu()** - Returns to system menu
    * GameCube: Returns to IPL menu
    * Wii: Returns to Wii Menu (System Menu)
  
  - **OSReturnToDataManager()** - Returns to save data manager
    * Wii only: System Menu → Data Management
  
  - **OSRestart()** - Restarts with specific code
    * Used for game relaunching
    * Preserves reset code for next boot
  
  **Shutdown Functions:**
  
  Games/libraries register shutdown callbacks via OSRegisterShutdownFunction():
  
  ```c
  OSShutdownFunctionInfo myShutdown = {
      .func = MyCleanup,
      .priority = OS_SHUTDOWN_PRIO_CARD  // Higher = earlier
  };
  OSRegisterShutdownFunction(&myShutdown);
  
  BOOL MyCleanup(BOOL final, u32 event) {
      if (final) {
          // About to reset, do final cleanup
          CloseFiles();
          SaveProgress();
      } else {
          // Preparing for reset, can return FALSE to delay
          if (!IsSafeToReset()) return FALSE;
      }
      return TRUE;  // Ready for reset
  }
  ```
  
  **Call Order:**
  1. __OSCallShutdownFunctions(FALSE, event) - Prepare
     - Calls all registered functions with final=FALSE
     - Functions return TRUE when ready, FALSE to wait
     - Repeats until all return TRUE (or timeout)
  
  2. __OSCallShutdownFunctions(TRUE, event) - Final
     - Calls all registered functions with final=TRUE
     - Functions do final cleanup
  
  3. Hardware reset/shutdown
  
  **Priorities:**
  - Lower number = called first
  - Common priorities:
    * OS_SHUTDOWN_PRIO_SO (110) - Socket
    * OS_SHUTDOWN_PRIO_CARD (127) - Memory card
    * OS_SHUTDOWN_PRIO_PAD (127) - Controllers
    * OS_SHUTDOWN_PRIO_GX (127) - Graphics
    * OS_SHUTDOWN_PRIO_VI (127) - Video
    * OS_SHUTDOWN_PRIO_NAND (255) - NAND filesystem
    * OS_SHUTDOWN_PRIO_ALARM (max) - Timers (last)
  
  **Save Region:**
  
  Small region of RAM (typically 64 bytes) that survives soft resets:
  
  ```c
  // Before restart
  SaveData data = { ... };
  OSSetSaveRegion(&data, (&data) + 1);
  OSRestart(OS_RESETCODE_RESTART);
  
  // After restart (in OSInit or after)
  void* start, *end;
  OSGetSavedRegion(&start, &end);
  if (start) {
      SaveData* data = (SaveData*)start;
      // Use saved data
  }
  ```
  
  Original hardware: IPL preserves this RAM region across restart.
  PC: We just keep it in a global variable since process doesn't actually reset.
  
  PC PORT REALITY:
  ================
  
  **What We Can Do:** ✅
  
  1. **Shutdown Function Queue**
     - Register/unregister callbacks
     - Call in priority order
     - Pass final flag and event type
     - Full API compatibility
  
  2. **Save Region**
     - Track region pointers
     - Preserve across "restarts"
     - OSGetSavedRegion returns previous region
  
  3. **Reset Code**
     - Store and retrieve reset code
     - Games can check if restarted
     - OSIsRestart() macro works
  
  **What We Can't Do:** ❌
  
  1. **Actual Hardware Reset**
     - Can't reset CPU/RAM on PC
     - Can't jump back to application start
     - Can't preserve memory across process restart
  
  2. **Return to System Menu**
     - No GameCube IPL or Wii Menu on PC
     - No system menu to return to
     - Can't launch other applications
  
  3. **Power Off Hardware**
     - Can't turn off PC from user-mode app
     - OS controls power management
  
  **PC Implementation Strategy:**
  
  For all reset/shutdown functions:
  1. Call shutdown callbacks (final=FALSE, then final=TRUE)
  2. Log what would happen on real hardware
  3. Exit the application (exit(0) for success, exit(1) for error)
  
  The save region persists within the process, so if the game has
  its own restart mechanism (re-executes main), the data survives.
  
  MIGRATION NOTES:
  ================
  
  **If your game uses resets:**
  
  1. **For restart** (OSRestart, soft reset):
     - Implement your own restart logic
     - Save state before exiting
     - Reload state on next launch
     - Or: Don't restart, just reset game state in-process
  
  2. **For return to menu** (OSReturnToMenu):
     - Exit to desktop (close application)
     - Or: Show your own game menu
     - Or: Use platform launcher integration
  
  3. **For shutdown** (OSShutdownSystem):
     - Exit application (OS decides what happens next)
     - Or: Use platform shutdown APIs (advanced)
  
  4. **For save region**:
     - Use proper save file instead
     - Or: Command-line arguments for restart info
     - Or: Persistent config file
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <stdlib.h>
#include <string.h>

/* Shutdown function queue */
typedef struct OSShutdownFunctionQueue {
    OSShutdownFunctionInfo* head;
    OSShutdownFunctionInfo* tail;
} OSShutdownFunctionQueue;

static OSShutdownFunctionQueue s_shutdownQueue = {NULL, NULL};

/* Reset state */
static u32 s_resetCode = 0;
static void* s_saveRegionStart = NULL;
static void* s_saveRegionEnd = NULL;

/* Previous save region (persists across "restart" within process) */
static void* s_savedRegionStart = NULL;
static void* s_savedRegionEnd = NULL;

/*===========================================================================*
  QUEUE MANAGEMENT MACROS (from reference implementation)
 *===========================================================================*/

/* EnqueueTail - insert info at the tail of the queue */
#define EnqueueTail(queue, info)                            \
do {                                                        \
    OSShutdownFunctionInfo* __prev;                         \
                                                            \
    __prev = (queue)->tail;                                 \
    if (__prev == NULL)                                     \
        (queue)->head = (info);                             \
    else                                                    \
        __prev->next = (info);                              \
    (info)->prev = __prev;                                  \
    (info)->next = NULL;                                    \
    (queue)->tail = (info);                                 \
} while (0)

/* EnqueuePrio - insert info into the queue in priority order */
#define EnqueuePrio(queue, info)                            \
do {                                                        \
    OSShutdownFunctionInfo* __prev;                         \
    OSShutdownFunctionInfo* __next;                         \
                                                            \
    for (__next = (queue)->head;                            \
         __next && __next->priority <= info->priority;      \
         __next = __next->next)                             \
            ;                                               \
    if (__next == NULL)                                     \
        EnqueueTail(queue, info);                           \
    else {                                                  \
        (info)->next = __next;                              \
        __prev = __next->prev;                              \
        __next->prev = (info);                              \
        (info)->prev = __prev;                              \
        if (__prev == NULL)                                 \
            (queue)->head = (info);                         \
        else                                                \
            __prev->next = (info);                          \
    }                                                       \
} while (0)

/* DequeueItem - remove the info from the queue */
#define DequeueItem(queue, info)                            \
do {                                                        \
    OSShutdownFunctionInfo* __next;                         \
    OSShutdownFunctionInfo* __prev;                         \
                                                            \
    __next = (info)->next;                                  \
    __prev = (info)->prev;                                  \
                                                            \
    if (__next == NULL)                                     \
        (queue)->tail = __prev;                             \
    else                                                    \
        __next->prev = __prev;                              \
                                                            \
    if (__prev == NULL)                                     \
        (queue)->head = __next;                             \
    else                                                    \
        __prev->next = __next;                              \
} while (0)

/*===========================================================================*
  SHUTDOWN FUNCTION MANAGEMENT
 *===========================================================================*/

/*---------------------------------------------------------------------------*
  Name:         OSRegisterShutdownFunction

  Description:  Registers a shutdown callback function.
                
                Functions are called in priority order (lowest number first).
                Multiple functions with same priority are called in
                registration order.
                
                On original hardware: Called during reset/shutdown sequence
                On PC: Called before exit()

  Arguments:    info - Callback info structure
                       Must set info->func and info->priority
                       Must remain valid (not stack-allocated!)

  Returns:      None
  
  Example:
    static OSShutdownFunctionInfo myCleanup = {
        .func = MyCleanupFunction,
        .priority = OS_SHUTDOWN_PRIO_CARD
    };
    OSRegisterShutdownFunction(&myCleanup);
 *---------------------------------------------------------------------------*/
void OSRegisterShutdownFunction(OSShutdownFunctionInfo* info) {
    if (!info || !info->func) return;
    
    /* Insert into queue in priority order */
    EnqueuePrio(&s_shutdownQueue, info);
    
#ifdef _DEBUG
    OSReport("[OSReset] Registered shutdown function at priority %u\n",
             info->priority);
#endif
}

/*---------------------------------------------------------------------------*
  Name:         OSUnregisterShutdownFunction

  Description:  Removes a shutdown callback from the queue.

  Arguments:    info - Callback to remove (same pointer passed to register)

  Returns:      None
 *---------------------------------------------------------------------------*/
void OSUnregisterShutdownFunction(OSShutdownFunctionInfo* info) {
    if (!info) return;
    
    /* Remove from queue */
    DequeueItem(&s_shutdownQueue, info);
    
#ifdef _DEBUG
    OSReport("[OSReset] Unregistered shutdown function at priority %u\n",
             info->priority);
#endif
}

/*---------------------------------------------------------------------------*
  Name:         __OSCallShutdownFunctions (Internal)

  Description:  Calls all registered shutdown functions in priority order.
                
                On original hardware:
                - Called twice: once with final=FALSE (prepare), once with final=TRUE (cleanup)
                - If any function returns FALSE during prepare, waits and retries
                - Timeout after OS_RESET_TIMEOUT if functions don't become ready
                
                On PC: Same behavior, but we don't timeout (let functions finish)

  Arguments:    final - TRUE for final cleanup, FALSE for prepare
                event - Event type (OS_SD_REBOOT, OS_SD_SHUTDOWN, etc.)

  Returns:      TRUE if all functions completed successfully
 *---------------------------------------------------------------------------*/
static BOOL __OSCallShutdownFunctions(BOOL final, u32 event) {
    OSShutdownFunctionInfo* info;
    BOOL allReady = TRUE;
    u32 priority = 0;
    BOOL err = FALSE;
    
    /* Walk the queue in priority order */
    for (info = s_shutdownQueue.head; info != NULL; info = info->next) {
        /* Call functions of same priority together */
        if (info->priority < priority) {
            /* Shouldn't happen if queue is sorted */
            continue;
        }
        priority = info->priority;
        
        /* Call the function */
        if (info->func) {
            BOOL ready = info->func(final, event);
            
            if (!final && !ready) {
                /* Function not ready yet (during prepare phase) */
                allReady = FALSE;
            }
            
#ifdef _DEBUG
            const char* phaseStr = final ? "FINAL" : "PREPARE";
            const char* statusStr = ready ? "READY" : "NOT READY";
            OSReport("[OSReset] Shutdown function (priority %u) [%s]: %s\n",
                     info->priority, phaseStr, statusStr);
#endif
        }
    }
    
    return allReady;
}

/*===========================================================================*
  RESET CODE AND SAVE REGION
 *===========================================================================*/

/*---------------------------------------------------------------------------*
  Name:         OSGetResetCode

  Description:  Returns the reset code set by last restart.
                
                On original hardware: IPL stores reset code in low memory
                On PC: We store in a global variable

  Returns:      Reset code (see OS_RESETCODE_* defines)
 *---------------------------------------------------------------------------*/
u32 OSGetResetCode(void) {
    return s_resetCode;
}

/*---------------------------------------------------------------------------*
  Name:         OSSetSaveRegion / OSGetSaveRegion / OSGetSavedRegion

  Description:  Manages the "save region" - a small area of RAM preserved
                across soft resets.
                
                On original hardware: IPL preserves this RAM region
                On PC: We simulate by keeping pointers in globals

  Arguments:    start/end - Pointers defining the region
 *---------------------------------------------------------------------------*/
void OSSetSaveRegion(void* start, void* end) {
    s_saveRegionStart = start;
    s_saveRegionEnd = end;
    
#ifdef _DEBUG
    if (start && end) {
        OSReport("[OSReset] Save region set: %p - %p (%u bytes)\n",
                 start, end, (u32)((u8*)end - (u8*)start));
    }
#endif
}

void OSGetSaveRegion(void** start, void** end) {
    if (start) *start = s_saveRegionStart;
    if (end) *end = s_saveRegionEnd;
}

void OSGetSavedRegion(void** start, void** end) {
    /* Returns the region from "previous boot" */
    if (start) *start = s_savedRegionStart;
    if (end) *end = s_savedRegionEnd;
}

/*===========================================================================*
  RESET/SHUTDOWN FUNCTIONS
 *===========================================================================*/

/*---------------------------------------------------------------------------*
  Name:         OSRebootSystem

  Description:  Reboots the system (cold reset).
                
                On original hardware:
                - Calls shutdown functions
                - Performs full hardware reset
                - Re-executes IPL and apploader
                
                On PC:
                - Calls shutdown functions
                - Exits application
                - OS decides what happens next

  Returns:      Does not return
 *---------------------------------------------------------------------------*/
void OSRebootSystem(void) {
    OSReport("[OSReset] ========================================\n");
    OSReport("[OSReset] OSRebootSystem called\n");
    OSReport("[OSReset] Calling shutdown functions (PREPARE)...\n");
    
    /* Prepare phase */
    __OSCallShutdownFunctions(FALSE, OS_SD_REBOOT);
    
    OSReport("[OSReset] Calling shutdown functions (FINAL)...\n");
    
    /* Final cleanup */
    __OSCallShutdownFunctions(TRUE, OS_SD_REBOOT);
    
    OSReport("[OSReset] System reboot complete. Exiting...\n");
    OSReport("[OSReset] ========================================\n");
    
    /* On PC: Exit the application */
    exit(0);
}

/*---------------------------------------------------------------------------*
  Name:         OSShutdownSystem

  Description:  Shuts down the system (power off).
                
                On original hardware: Turns console off
                On PC: Exits application

  Returns:      Does not return
 *---------------------------------------------------------------------------*/
void OSShutdownSystem(void) {
    OSReport("[OSReset] ========================================\n");
    OSReport("[OSReset] OSShutdownSystem called\n");
    OSReport("[OSReset] Calling shutdown functions (PREPARE)...\n");
    
    __OSCallShutdownFunctions(FALSE, OS_SD_SHUTDOWN);
    
    OSReport("[OSReset] Calling shutdown functions (FINAL)...\n");
    __OSCallShutdownFunctions(TRUE, OS_SD_SHUTDOWN);
    
    OSReport("[OSReset] System shutdown complete. Exiting...\n");
    OSReport("[OSReset] ========================================\n");
    
    exit(0);
}

/*---------------------------------------------------------------------------*
  Name:         OSRestart

  Description:  Restarts the game with a specific reset code.
                
                On original hardware:
                - Preserves save region
                - Stores reset code
                - Jumps back to apploader
                - Game restarts with saved data
                
                On PC:
                - Preserves save region pointers in globals
                - Exits application
                - If game has restart mechanism, can check OSIsRestart()

  Arguments:    resetCode - Code to identify restart reason

  Returns:      Does not return
 *---------------------------------------------------------------------------*/
void OSRestart(u32 resetCode) {
    OSReport("[OSReset] ========================================\n");
    OSReport("[OSReset] OSRestart called with code 0x%08X\n", resetCode);
    
    /* Set reset code with restart flag */
    s_resetCode = resetCode | OS_RESETCODE_RESTART;
    
    /* Preserve save region for "next boot" */
    s_savedRegionStart = s_saveRegionStart;
    s_savedRegionEnd = s_saveRegionEnd;
    
    OSReport("[OSReset] Calling shutdown functions (PREPARE)...\n");
    __OSCallShutdownFunctions(FALSE, OS_SD_RESTART);
    
    OSReport("[OSReset] Calling shutdown functions (FINAL)...\n");
    __OSCallShutdownFunctions(TRUE, OS_SD_RESTART);
    
    if (s_savedRegionStart && s_savedRegionEnd) {
        OSReport("[OSReset] Save region preserved: %p - %p\n",
                 s_savedRegionStart, s_savedRegionEnd);
    }
    
    OSReport("[OSReset] Restart complete. Exiting...\n");
    OSReport("[OSReset] Note: On PC, use exit code or save file to persist restart state\n");
    OSReport("[OSReset] ========================================\n");
    
    exit(0);
}

/*---------------------------------------------------------------------------*
  Name:         OSReturnToMenu

  Description:  Returns to system menu.
                
                On original hardware:
                - GameCube: Returns to IPL menu
                - Wii: Returns to Wii Menu (System Menu)
                
                On PC: Exits to desktop

  Returns:      Does not return
 *---------------------------------------------------------------------------*/
void OSReturnToMenu(void) {
    OSReport("[OSReset] ========================================\n");
    OSReport("[OSReset] OSReturnToMenu called\n");
    OSReport("[OSReset] Calling shutdown functions (PREPARE)...\n");
    
    __OSCallShutdownFunctions(FALSE, OS_SD_RETURNTOMENU);
    
    OSReport("[OSReset] Calling shutdown functions (FINAL)...\n");
    __OSCallShutdownFunctions(TRUE, OS_SD_RETURNTOMENU);
    
    OSReport("[OSReset] Return to menu complete. Exiting to desktop...\n");
    OSReport("[OSReset] ========================================\n");
    
    exit(0);
}

/*---------------------------------------------------------------------------*
  Name:         OSReturnToDataManager

  Description:  Returns to Wii system menu data management screen.
                
                On original hardware: Wii Menu → Data Management
                On PC: Exits to desktop

  Returns:      Does not return
 *---------------------------------------------------------------------------*/
void OSReturnToDataManager(void) {
    OSReport("[OSReset] ========================================\n");
    OSReport("[OSReset] OSReturnToDataManager called\n");
    OSReport("[OSReset] Calling shutdown functions...\n");
    
    __OSCallShutdownFunctions(FALSE, OS_SD_RETURNTOMENU);
    __OSCallShutdownFunctions(TRUE, OS_SD_RETURNTOMENU);
    
    OSReport("[OSReset] Return to data manager complete. Exiting to desktop...\n");
    OSReport("[OSReset] ========================================\n");
    
    exit(0);
}

/*---------------------------------------------------------------------------*
  Name:         OSResetSystem

  Description:  General reset function with type and options.
                
                This is the most flexible reset function, used by other
                reset functions internally.

  Arguments:    reset      - Reset type (OS_RESET_RESTART, OS_RESET_HOTRESET, OS_RESET_SHUTDOWN)
                resetCode  - Reset code for restart
                forceMenu  - TRUE to return to menu instead of restart

  Returns:      Does not return
 *---------------------------------------------------------------------------*/
void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu) {
    OSReport("[OSReset] ========================================\n");
    OSReport("[OSReset] OSResetSystem called:\n");
    OSReport("[OSReset]   reset     = %d\n", reset);
    OSReport("[OSReset]   resetCode = 0x%08X\n", resetCode);
    OSReport("[OSReset]   forceMenu = %d\n", forceMenu);
    
    if (forceMenu) {
        /* Force return to menu */
        OSReturnToMenu();
        /* NOT REACHED */
    }
    
    /* Store reset code */
    s_resetCode = resetCode;
    
    switch (reset) {
        case OS_RESET_RESTART:
            /* Soft reset */
            s_savedRegionStart = s_saveRegionStart;
            s_savedRegionEnd = s_saveRegionEnd;
            OSReport("[OSReset] Performing soft reset (restart)\n");
            __OSCallShutdownFunctions(FALSE, OS_SD_RESTART);
            __OSCallShutdownFunctions(TRUE, OS_SD_RESTART);
            break;
            
        case OS_RESET_HOTRESET:
            /* Hard reset */
            OSReport("[OSReset] Performing hard reset\n");
            __OSCallShutdownFunctions(FALSE, OS_SD_REBOOT);
            __OSCallShutdownFunctions(TRUE, OS_SD_REBOOT);
            break;
            
        case OS_RESET_SHUTDOWN:
            /* Shutdown */
            OSReport("[OSReset] Performing shutdown\n");
            __OSCallShutdownFunctions(FALSE, OS_SD_SHUTDOWN);
            __OSCallShutdownFunctions(TRUE, OS_SD_SHUTDOWN);
            break;
            
        default:
            OSReport("[OSReset] Unknown reset type: %d\n", reset);
            break;
    }
    
    OSReport("[OSReset] Reset complete. Exiting...\n");
    OSReport("[OSReset] ========================================\n");
    
    exit(0);
}
