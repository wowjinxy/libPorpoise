/*---------------------------------------------------------------------------*
  OSInterrupt.c - Interrupt Management
  
  HARDWARE INTERRUPT SYSTEM (GC/Wii):
  ====================================
  
  The original GameCube/Wii had a sophisticated hardware interrupt system:
  
  **Hardware Sources (28 interrupt types):**
  - **MEM (Memory):** Memory interface errors, address errors
  - **DSP:** Audio DSP completion, ARAM DMA
  - **AI:** Audio interface streaming
  - **EXI (External Interface):** Memory cards, network adapter, modems
  - **PI (Processor Interface):** Graphics, Video, Serial, Disc, Debug
    * CP (Command Processor) - GX graphics FIFO
    * PE (Pixel Engine) - Graphics rendering completion
    * VI (Video Interface) - Vertical retrace
    * SI (Serial Interface) - Controller polling
    * DI (Disc Interface) - DVD reads
    * RSW/HSP - Reset switch, high-speed port
    * ACR (Wii) - IPC for ARM coprocessor (Starlet/IOS)
  
  **Priority-Based Dispatch:**
  - Hardware interrupts go through Flipper/Hollywood chips
  - Gekko/Broadway CPU receives external interrupt exception
  - __OSDispatchInterrupt() prioritizes and dispatches to handlers
  - Uses hardware registers (PI_INTSR, PI_INTMSK, etc.)
  
  **MSR Register Control:**
  - Interrupts enabled/disabled via MSR[EE] (External Enable bit)
  - Assembly code: mfmsr/mtmsr instructions
  - Atomic operations (OSDisableInterrupts = save MSR + clear EE bit)
  
  **Typical Game Usage:**
  ```c
  // Register handler for vertical retrace
  __OSSetInterruptHandler(__OS_INTERRUPT_PI_VI, MyVIHandler);
  __OSUnmaskInterrupts(OS_INTERRUPTMASK_PI_VI);
  
  // Handler gets called 60 times per second (NTSC)
  void MyVIHandler(__OSInterrupt interrupt, OSContext* context) {
      // Update frame counter, trigger render, etc.
  }
  ```
  
  PC PORT REALITY:
  ================
  
  **Why We Can't Port Interrupts Directly:**
  
  1. **No Hardware** ❌
     - No Flipper/Hollywood chips on PC
     - No PI/VI/EXI/SI/DI hardware registers
     - Can't read PI_INTSR or write PI_INTMSK
  
  2. **Different CPU** ❌
     - x86/ARM don't have PowerPC MSR register
     - Can't use mfmsr/mtmsr instructions
     - OS controls interrupt enable/disable (privileged)
  
  3. **OS-Level Interrupts** ❌
     - Windows/Linux handle hardware interrupts at kernel level
     - User-mode apps can't intercept hardware interrupts
     - Can't install interrupt handlers like on bare-metal GC/Wii
  
  4. **No Decrementer** ❌
     - PowerPC has hardware decrementer register
     - Generates interrupt when reaches zero
     - Used for timing, preemption, alarms
     - PC uses OS timer APIs instead
  
  **What We CAN Do:**
  
  1. **Track Handler Registration** ✅
     - Store handlers in table (API compatibility)
     - Games can register handlers
     - Handlers just won't be called by hardware
  
  2. **Manual Invocation** ✅
     - Games can manually call handlers:
       ```c
       __OSInterruptHandler handler = __OSGetInterruptHandler(__OS_INTERRUPT_PI_VI);
       if (handler) handler(__OS_INTERRUPT_PI_VI, &context);
       ```
  
  3. **Polling Replacement** ✅
     - Instead of VI interrupt → poll vsync
     - Instead of DI interrupt → blocking read
     - Instead of EXI interrupt → blocking I/O
  
  MIGRATION STRATEGIES:
  =====================
  
  **Strategy 1: Polling (Simple)**
  ```c
  // Old (Interrupt-driven):
  __OSSetInterruptHandler(__OS_INTERRUPT_PI_VI, OnVBlank);
  __OSUnmaskInterrupts(OS_INTERRUPTMASK_PI_VI);
  
  // New (Polling):
  while (running) {
      WaitForVSync();  // Platform-specific (SDL, GLFW, etc.)
      OnVBlank();      // Call directly
  }
  ```
  
  **Strategy 2: Callbacks (Medium)**
  ```c
  // Register callbacks that game framework calls
  void SetVBlankCallback(void (*callback)(void)) {
      // Store callback, call from main loop
  }
  ```
  
  **Strategy 3: Threading (Advanced)**
  ```c
  // Use a background thread to simulate hardware timing
  void VBlankThread(void* arg) {
      while (running) {
          SleepMilliseconds(16);  // ~60 Hz
          
          __OSInterruptHandler handler = __OSGetInterruptHandler(__OS_INTERRUPT_PI_VI);
          if (handler) {
              OSContext ctx;
              handler(__OS_INTERRUPT_PI_VI, &ctx);
          }
      }
  }
  ```
  
  **Strategy 4: Event-Driven (Best)**
  ```c
  // Modern graphics APIs have event callbacks
  // OpenGL: SwapBuffers with vsync
  // Vulkan: vkQueuePresentKHR
  // DirectX: IDXGISwapChain::Present with VSYNC_INTERVAL
  ```
  
  IMPLEMENTATION SUMMARY:
  =======================
  
  This file provides:
  - ✅ Handler registration (full compatibility)
  - ✅ Interrupt mask tracking (stubs, API compatible)
  - ✅ Enable/Disable interrupt tracking (stubs)
  - ❌ Actual hardware interrupt handling (impossible on PC)
  - 📖 Complete documentation for migration
  
  Most games should:
  1. Use manual callbacks from main loop (VI)
  2. Use blocking I/O instead of DI interrupts
  3. Remove decrementer usage (use OSAlarm or timers)
  4. Poll controllers instead of SI interrupts
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#if defined(_MSC_VER)
#define PORPOISE_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define PORPOISE_THREAD_LOCAL __thread
#else
#define PORPOISE_THREAD_LOCAL
#endif

/* Interrupt handler table */
static __OSInterruptHandler s_interruptHandlers[__OS_INTERRUPT_MAX] = {NULL};

/* Interrupt masks (for API compatibility) */
static OSInterruptMask s_globalMask = 0xFFFFFFFF;  /* All masked initially */
static OSInterruptMask s_currentMask = 0xFFFFFFFF;
static BOOL s_interruptsEnabled = TRUE;
static PORPOISE_THREAD_LOCAL u32 s_tlsInterruptDisableDepth = 0;

#ifdef _WIN32
static CRITICAL_SECTION s_interruptLock;
static INIT_ONCE s_interruptLockInitOnce = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK InitInterruptLockOnce(PINIT_ONCE once, PVOID param, PVOID* context) {
    (void)once;
    (void)param;
    (void)context;
    InitializeCriticalSection(&s_interruptLock);
    return TRUE;
}

static void EnsureInterruptLockInitialized(void) {
    InitOnceExecuteOnce(&s_interruptLockInitOnce, InitInterruptLockOnce, NULL, NULL);
}

static void InterruptLockEnter(void) {
    EnterCriticalSection(&s_interruptLock);
}

static void InterruptLockLeave(void) {
    LeaveCriticalSection(&s_interruptLock);
}
#else
static pthread_mutex_t s_interruptLock;
static pthread_once_t s_interruptLockInitOnce = PTHREAD_ONCE_INIT;

static void InitInterruptLockOnce(void) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    #if 0
    //TODO: Not compiling
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    #endif
    pthread_mutex_init(&s_interruptLock, &attr);
    pthread_mutexattr_destroy(&attr);
}

static void EnsureInterruptLockInitialized(void) {
    pthread_once(&s_interruptLockInitOnce, InitInterruptLockOnce);
}

static void InterruptLockEnter(void) {
    pthread_mutex_lock(&s_interruptLock);
}

static void InterruptLockLeave(void) {
    pthread_mutex_unlock(&s_interruptLock);
}
#endif

/* Debug: Interrupt names for logging */
#ifdef _DEBUG
static const char* s_interruptNames[__OS_INTERRUPT_MAX] = {
    "MEM_0",            /*  0 */
    "MEM_1",            /*  1 */
    "MEM_2",            /*  2 */
    "MEM_3",            /*  3 */
    "MEM_ADDRESS",      /*  4 */
    "DSP_AI",           /*  5 */
    "DSP_ARAM",         /*  6 */
    "DSP_DSP",          /*  7 */
    "AI_AI",            /*  8 */
    "EXI_0_EXI",        /*  9 */
    "EXI_0_TC",         /* 10 */
    "EXI_0_EXT",        /* 11 */
    "EXI_1_EXI",        /* 12 */
    "EXI_1_TC",         /* 13 */
    "EXI_1_EXT",        /* 14 */
    "EXI_2_EXI",        /* 15 */
    "EXI_2_TC",         /* 16 */
    "PI_CP",            /* 17 - Command Processor (GX) */
    "PI_PE_TOKEN",      /* 18 - Pixel Engine token */
    "PI_PE_FINISH",     /* 19 - Pixel Engine finish */
    "PI_SI",            /* 20 - Serial Interface (controllers) */
    "PI_DI",            /* 21 - Disc Interface (DVD) */
    "PI_RSW",           /* 22 - Reset switch */
    "PI_ERROR",         /* 23 - Processor interface error */
    "PI_VI",            /* 24 - Video Interface (vsync) */
    "PI_DEBUG",         /* 25 - Debug interrupt */
    "PI_HSP",           /* 26 - High-speed port */
    "PI_ACR",           /* 27 - ARM coprocessor (Wii IOS) */
    "UNKNOWN_28",
    "UNKNOWN_29",
    "UNKNOWN_30",
    "UNKNOWN_31",
};
#endif

/*---------------------------------------------------------------------------*
  Name:         __OSSetInterruptHandler

  Description:  Registers an interrupt handler for a specific interrupt type.
                
                On original hardware: Handler is called when hardware interrupt
                occurs. Dispatcher checks PI_INTSR, routes to correct handler.
                
                On PC: Handler is registered but NOT automatically called.
                Games must manually invoke handlers or use polling/callbacks.

  Arguments:    interrupt - Interrupt type (__OS_INTERRUPT_PI_VI, etc.)
                handler   - Function to call

  Returns:      Previous handler (NULL if none)
 *---------------------------------------------------------------------------*/
__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, 
                                             __OSInterruptHandler handler) {
    if (interrupt < 0 || interrupt >= __OS_INTERRUPT_MAX) {
        return NULL;
    }
    
    __OSInterruptHandler old = s_interruptHandlers[interrupt];
    s_interruptHandlers[interrupt] = handler;
    
#ifdef _DEBUG
    if (handler) {
        OSReport("[OSInterrupt] Registered handler for %s (interrupt %d)\n",
                 s_interruptNames[interrupt], interrupt);
        OSReport("              NOTE: Handler must be called manually on PC!\n");
    }
#endif
    
    return old;
}

/*---------------------------------------------------------------------------*
  Name:         __OSGetInterruptHandler

  Description:  Retrieves the registered handler for an interrupt type.

  Arguments:    interrupt - Interrupt type

  Returns:      Handler function (NULL if none registered)
 *---------------------------------------------------------------------------*/
__OSInterruptHandler __OSGetInterruptHandler(__OSInterrupt interrupt) {
    if (interrupt < 0 || interrupt >= __OS_INTERRUPT_MAX) {
        return NULL;
    }
    
    return s_interruptHandlers[interrupt];
}

/*---------------------------------------------------------------------------*
  Name:         OSGetInterruptMask

  Description:  Returns the current interrupt mask.
                
                On original hardware: Reads from PI_INTMSK register and
                OS low memory mask. Bits set = interrupt is MASKED (disabled).
                
                On PC: Returns stored mask value (not connected to hardware).

  Returns:      Current interrupt mask
 *---------------------------------------------------------------------------*/
OSInterruptMask OSGetInterruptMask(void) {
    /* Original hardware:
     * - Reads PI_INTMSK hardware register
     * - Reads OS low memory mask (OS_INTERRUPTMASK_ADDR)
     * - Combines module and global masks
     * 
     * PC: Just return our tracked mask.
     */
    return s_currentMask;
}

/*---------------------------------------------------------------------------*
  Name:         OSSetInterruptMask

  Description:  Sets the interrupt mask.
                
                On original hardware: Writes to PI_INTMSK register and
                OS low memory. Also updates MEM/DSP/AI/EXI module masks.
                
                On PC: STUB - Stores mask but doesn't affect actual interrupts
                (because there are no hardware interrupts on PC).

  Arguments:    mask - New interrupt mask (bits set = masked)

  Returns:      Previous interrupt mask
 *---------------------------------------------------------------------------*/
OSInterruptMask OSSetInterruptMask(OSInterruptMask mask) {
    /* Original hardware:
     * - Writes to hardware registers (PI_INTMSK, etc.)
     * - Updates module-level masks (MEM, DSP, AI, EXI, PI)
     * - Affects which hardware interrupts can reach CPU
     * 
     * PC: No hardware, just track the mask value.
     */
    OSInterruptMask prev = s_currentMask;
    s_currentMask = mask;
    
#ifdef _DEBUG
    if (prev != mask) {
        OSReport("[OSInterrupt] Mask changed: 0x%08X → 0x%08X\n", prev, mask);
    }
#endif
    
    return prev;
}

/*---------------------------------------------------------------------------*
  Name:         __OSMaskInterrupts

  Description:  Masks (disables) specified interrupts.
                
                On original hardware: Sets mask bits in hardware registers.
                Masked interrupts won't trigger exception handler.
                
                On PC: STUB - Tracks mask state only.

  Arguments:    mask - Interrupts to mask (bits set = mask those interrupts)

  Returns:      Previous interrupt mask
 *---------------------------------------------------------------------------*/
OSInterruptMask __OSMaskInterrupts(OSInterruptMask mask) {
    /* Original hardware:
     * - ORs mask bits into current mask (set bits = masked)
     * - Updates PI_INTMSK hardware register
     * - Masked interrupts don't generate exceptions
     * 
     * PC: Simulate by OR-ing into our mask.
     */
    OSInterruptMask prev = s_currentMask;
    s_currentMask |= mask;
    
#ifdef _DEBUG
    if (mask != 0) {
        OSReport("[OSInterrupt] Masked interrupts: 0x%08X (new mask: 0x%08X)\n", 
                 mask, s_currentMask);
    }
#endif
    
    return prev;
}

/*---------------------------------------------------------------------------*
  Name:         __OSUnmaskInterrupts

  Description:  Unmasks (enables) specified interrupts.
                
                On original hardware: Clears mask bits in hardware registers.
                Unmasked interrupts can now trigger exception handler.
                
                On PC: STUB - Tracks mask state only.

  Arguments:    mask - Interrupts to unmask (bits set = unmask those interrupts)

  Returns:      Previous interrupt mask
 *---------------------------------------------------------------------------*/
OSInterruptMask __OSUnmaskInterrupts(OSInterruptMask mask) {
    /* Original hardware:
     * - Clears mask bits from current mask (cleared bits = unmasked)
     * - Updates PI_INTMSK hardware register
     * - Unmasked interrupts can generate exceptions
     * 
     * PC: Simulate by clearing bits from our mask.
     */
    OSInterruptMask prev = s_currentMask;
    s_currentMask &= ~mask;
    
#ifdef _DEBUG
    if (mask != 0) {
        OSReport("[OSInterrupt] Unmasked interrupts: 0x%08X (new mask: 0x%08X)\n", 
                 mask, s_currentMask);
    }
#endif
    
    return prev;
}

/*===========================================================================*
  CPU INTERRUPT ENABLE/DISABLE (NOT IMPLEMENTABLE ON PC)
  
  These functions manipulate the PowerPC MSR[EE] bit to enable/disable
  external interrupts at the CPU level. On x86/ARM, this requires kernel
  mode and isn't accessible from user-mode applications.
  
  We track the state for API compatibility but can't actually disable
  interrupts on PC.
 *===========================================================================*/

/*---------------------------------------------------------------------------*
  Name:         OSDisableInterrupts

  Description:  Disables CPU external interrupts.
                
                On original hardware (PowerPC assembly):
                ```
                mfmsr   r3              # Read MSR
                rlwinm  r4, r3, 0, 17, 15  # Clear EE bit
                mtmsr   r4              # Write MSR (interrupts off)
                rlwinm  r3, r3, 17, 31, 31 # Return old EE bit
                blr
                ```
                
                On PC: CAN'T disable interrupts (kernel-level operation).
                We track state for API compatibility.

  Returns:      TRUE if interrupts were enabled, FALSE otherwise
 *---------------------------------------------------------------------------*/
BOOL OSDisableInterrupts(void) {
    BOOL wasEnabled = (s_tlsInterruptDisableDepth == 0);
    s_tlsInterruptDisableDepth++;
    s_interruptsEnabled = FALSE;
    return wasEnabled;
}

/*---------------------------------------------------------------------------*
  Name:         OSEnableInterrupts

  Description:  Enables CPU external interrupts.
                
                On original hardware (PowerPC assembly):
                ```
                mfmsr   r3        # Read MSR
                ori     r4, r3, 0x8000  # Set EE bit
                mtmsr   r4        # Write MSR (interrupts on)
                rlwinm  r3, r3, 17, 31, 31  # Return old EE bit
                blr
                ```
                
                On PC: CAN'T control interrupt enable (kernel-level).

  Returns:      TRUE if interrupts were enabled, FALSE otherwise
 *---------------------------------------------------------------------------*/
BOOL OSEnableInterrupts(void) {
    BOOL wasEnabled = (s_tlsInterruptDisableDepth == 0);
    s_tlsInterruptDisableDepth = 0;
    s_interruptsEnabled = TRUE;
    return wasEnabled;
}

/*---------------------------------------------------------------------------*
  Name:         OSRestoreInterrupts

  Description:  Restores interrupt enable state.
                
                Pattern on original hardware:
                ```c
                BOOL enabled = OSDisableInterrupts();
                // Critical section
                OSRestoreInterrupts(enabled);
                ```
                
                On PC: Use mutexes instead:
                ```c
                OSLockMutex(&mutex);
                // Critical section
                OSUnlockMutex(&mutex);
                ```

  Arguments:    level - Previous state from OSDisableInterrupts/OSEnableInterrupts

  Returns:      Previous state (before this call)
 *---------------------------------------------------------------------------*/
BOOL OSRestoreInterrupts(BOOL level) {
    BOOL prev = (s_tlsInterruptDisableDepth == 0);

    /* Unwind one disable level for this restore call. */
    if (s_tlsInterruptDisableDepth > 0) {
        s_tlsInterruptDisableDepth--;
    }

    if (level) {
        s_tlsInterruptDisableDepth = 0;
    } else if (s_tlsInterruptDisableDepth == 0) {
        s_tlsInterruptDisableDepth = 1;
    }

    s_interruptsEnabled = (s_tlsInterruptDisableDepth == 0) ? TRUE : FALSE;
    return prev;
}

/*---------------------------------------------------------------------------*
  Internal helpers used by OSSleepThread/OSWakeupThread implementation.
 *---------------------------------------------------------------------------*/
u32 __OSSuspendInterruptLockForSleep(void) {
    u32 released = s_tlsInterruptDisableDepth;
    s_tlsInterruptDisableDepth = 0;
    s_interruptsEnabled = TRUE;
    return released;
}

void __OSResumeInterruptLockAfterSleep(u32 depth) {
    s_tlsInterruptDisableDepth += depth;
    s_interruptsEnabled = (s_tlsInterruptDisableDepth == 0) ? TRUE : FALSE;
}

/*===========================================================================*
  HELPER UTILITIES
 *===========================================================================*/

/*---------------------------------------------------------------------------*
  Name:         __OSInterruptInit (Internal)

  Description:  Initializes interrupt system. Called by OSInit().
                
                On original hardware:
                - Clears interrupt handler table in low memory
                - Sets up PI_INTMSK register
                - Masks all interrupts initially
                - Installs ExternalInterruptHandler exception handler
                
                On PC: Just clears our handler table.

  Arguments:    None
 *---------------------------------------------------------------------------*/
void __OSInterruptInit(void) {
    EnsureInterruptLockInitialized();

    /* Clear handler table */
    memset(s_interruptHandlers, 0, sizeof(s_interruptHandlers));
    
    /* Start with all interrupts masked */
    s_currentMask = 0xFFFFFFFF;
    s_globalMask = 0xFFFFFFFF;
    s_interruptsEnabled = TRUE;
    
#ifdef _DEBUG
    OSReport("[OSInterrupt] Initialized (PC mode - handlers must be called manually)\n");
#endif
}

/*---------------------------------------------------------------------------*
  Name:         __OSDispatchInterrupt (NOT IMPLEMENTED)

  Description:  On original hardware, this is called by the external interrupt
                exception handler. It reads PI_INTSR to determine which hardware
                triggered the interrupt, then dispatches to the appropriate
                registered handler.
                
                On PC: NOT IMPLEMENTED - No hardware to dispatch from.
                
                If your game needs interrupts on PC, you must:
                1. Poll for events in main loop
                2. Manually call handlers when events occur
                3. Use background threads to simulate timing
 *---------------------------------------------------------------------------*/
#if 0
void __OSDispatchInterrupt(__OSException exception, OSContext* context) {
    /* Original hardware:
     * 1. Reads PI_INTSR (interrupt status register)
     * 2. Checks interrupt priority table
     * 3. Finds highest priority pending interrupt
     * 4. Calls registered handler
     * 5. Clears interrupt status bit
     * 6. Checks for thread reschedule
     * 
     * PC: No hardware registers to read. Not implemented.
     */
}
#endif
