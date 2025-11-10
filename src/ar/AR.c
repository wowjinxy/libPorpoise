/*---------------------------------------------------------------------------*
  AR.c - ARAM (Auxiliary RAM / Audio RAM) Device Driver
  
  ARCHITECTURAL DIFFERENCES: GC/Wii vs PC
  ========================================
  
  On GC/Wii (ARAM Hardware):
  ---------------------------
  - 16MB dedicated audio memory (separate from main RAM)
  - Accessed via DMA transfers only (can't access directly)
  - Connected to DSP for audio processing
  - Used for storing audio samples, sound effects, music
  - DMA transfers managed by DSP registers
  - Requires 32-byte alignment for DMA
  - Bottom 16KB reserved for OS use
  
  On PC (Simulated ARAM):
  -----------------------
  - Allocate 16MB from regular heap as "ARAM"
  - DMA operations are just memcpy()
  - No real separation from main RAM
  - Audio data can be in regular memory
  - Instant transfers (no DMA latency)
  - Alignment still enforced for compatibility
  
  WHY SIMULATE:
  - Games expect ARAM for audio data storage
  - Audio libraries expect to DMA to/from ARAM
  - Need address space separate from main RAM
  - Maintains original memory layout for compatibility
  
  WHAT WE PRESERVE:
  - Same API (ARInit, ARAlloc, ARStartDMA, etc.)
  - 16MB size
  - OS reserved area (16KB)
  - Allocation/free system
  - 32-byte alignment requirements
  
  WHAT'S DIFFERENT:
  - No real hardware - just heap allocation
  - DMA is instant memcpy (not asynchronous DMA)
  - No DSP integration (PC audio uses different system)
 *---------------------------------------------------------------------------*/

#include <dolphin/ar.h>
#include <dolphin/os.h>
#include <string.h>
#include <stdlib.h>

/*---------------------------------------------------------------------------*
    Internal State
 *---------------------------------------------------------------------------*/

static BOOL s_initialized = FALSE;
static void* s_aramBase = NULL;              // Simulated ARAM memory
static u32 s_aramSize = AR_INTERNAL_SIZE;    // 16MB
static u32 s_aramAllocated = AR_OS_RESERVED; // Start after OS area
static ARCallback s_dmaCallback = NULL;
static volatile BOOL s_dmaBusy = FALSE;

/*---------------------------------------------------------------------------*
  Name:         ARInit

  Description:  Initialize ARAM subsystem. Allocates simulated ARAM from
                heap and sets up allocation tracking.
                
                On GC/Wii: Probes ARAM size, sets up DSP registers
                On PC: Allocates 16MB buffer from heap

  Arguments:    stackIndexAddr  Pointer for allocation stack (can be NULL)
                numEntries      Number of allocation entries (unused on PC)

  Returns:      Base address of user ARAM (after OS reserved area)
 *---------------------------------------------------------------------------*/
u32 ARInit(u32* stackIndexAddr, u32 numEntries) {
    (void)stackIndexAddr;
    (void)numEntries;
    
    if (s_initialized) {
        return AR_OS_RESERVED;  // Return user base
    }
    
    OSReport("AR: Initializing ARAM subsystem...\n");
    
    // Allocate simulated ARAM
    s_aramBase = malloc(s_aramSize);
    if (!s_aramBase) {
        OSReport("AR: Failed to allocate ARAM!\n");
        return 0;
    }
    
    // Clear ARAM
    memset(s_aramBase, 0, s_aramSize);
    
    s_aramAllocated = AR_OS_RESERVED;  // OS uses first 16KB
    s_initialized = TRUE;
    
    OSReport("AR: ARAM initialized - %u bytes (%u MB)\n", 
             s_aramSize, s_aramSize / (1024 * 1024));
    OSReport("AR: User base address: 0x%08X\n", AR_OS_RESERVED);
    
    return AR_OS_RESERVED;
}

/*---------------------------------------------------------------------------*
  Name:         ARReset

  Description:  Reset ARAM subsystem.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void ARReset(void) {
    if (s_aramBase) {
        free(s_aramBase);
        s_aramBase = NULL;
    }
    
    s_initialized = FALSE;
    s_aramAllocated = AR_OS_RESERVED;
    s_dmaCallback = NULL;
    s_dmaBusy = FALSE;
}

/*---------------------------------------------------------------------------*
  Name:         ARGetSize

  Description:  Get total ARAM size.

  Arguments:    None

  Returns:      ARAM size in bytes (16MB)
 *---------------------------------------------------------------------------*/
u32 ARGetSize(void) {
    return s_aramSize;
}

/*---------------------------------------------------------------------------*
  Name:         ARGetInternalSize

  Description:  Get internal ARAM size (vs expansion ARAM).
                GameCube has 16MB internal, no expansion on retail.

  Arguments:    None

  Returns:      Internal ARAM size in bytes
 *---------------------------------------------------------------------------*/
u32 ARGetInternalSize(void) {
    return s_aramSize;  // All internal on GameCube
}

/*---------------------------------------------------------------------------*
  Name:         ARGetBaseAddress

  Description:  Get base address of user ARAM (after OS reserved area).

  Arguments:    None

  Returns:      User ARAM base address (0x4000 = 16KB offset)
 *---------------------------------------------------------------------------*/
u32 ARGetBaseAddress(void) {
    return AR_OS_RESERVED;
}

/*---------------------------------------------------------------------------*
  Name:         ARCheckInit

  Description:  Check if ARAM is initialized.

  Arguments:    None

  Returns:      TRUE if ARInit() has been called
 *---------------------------------------------------------------------------*/
BOOL ARCheckInit(void) {
    return s_initialized;
}

/*---------------------------------------------------------------------------*
  Name:         ARAlloc

  Description:  Allocate ARAM space. Simple bump allocator.
                
                On GC/Wii: Allocates from ARAM address space
                On PC: Allocates from simulated ARAM buffer

  Arguments:    length  Number of bytes to allocate

  Returns:      ARAM address, or 0xFFFFFFFF if out of space
 *---------------------------------------------------------------------------*/
u32 ARAlloc(u32 length) {
    if (!s_initialized) {
        return 0xFFFFFFFF;
    }
    
    // Align to 32 bytes
    length = (length + 31) & ~31;
    
    if (s_aramAllocated + length > s_aramSize) {
        OSReport("AR: Out of ARAM space (requested %u bytes)\n", length);
        return 0xFFFFFFFF;
    }
    
    u32 addr = s_aramAllocated;
    s_aramAllocated += length;
    
    return addr;
}

/*---------------------------------------------------------------------------*
  Name:         ARFree

  Description:  Free ARAM space (simple stack-based free).
                Only frees from top of allocation stack.

  Arguments:    length  Pointer to receive size freed

  Returns:      ARAM address that was freed
 *---------------------------------------------------------------------------*/
u32 ARFree(u32* length) {
    if (!s_initialized) {
        if (length) *length = 0;
        return 0;
    }
    
    u32 freed = s_aramAllocated - AR_OS_RESERVED;
    s_aramAllocated = AR_OS_RESERVED;
    
    if (length) {
        *length = freed;
    }
    
    return AR_OS_RESERVED;
}

/*---------------------------------------------------------------------------*
  Name:         ARClear

  Description:  Clear ARAM contents.

  Arguments:    flag  Non-zero to perform clear

  Returns:      None
 *---------------------------------------------------------------------------*/
void ARClear(u32 flag) {
    if (!s_initialized || !flag || !s_aramBase) {
        return;
    }
    
    memset(s_aramBase, 0, s_aramSize);
}

/*---------------------------------------------------------------------------*
  Name:         ARStartDMA

  Description:  Start DMA transfer between ARAM and main RAM.
                
                On GC/Wii: Configures DSP DMA registers, starts transfer
                On PC: Performs immediate memcpy, calls callback

  Arguments:    type         DMA type (AR_MRAM_TO_ARAM or AR_ARAM_TO_MRAM)
                mainmemAddr  Main memory address (must be 32-byte aligned)
                aramAddr     ARAM address (must be 32-byte aligned)
                length       Transfer length (must be multiple of 32)

  Returns:      None
 *---------------------------------------------------------------------------*/
void ARStartDMA(u32 type, u32 mainmemAddr, u32 aramAddr, u32 length) {
    if (!s_initialized || !s_aramBase) {
        return;
    }
    
    // Verify alignment
    if ((mainmemAddr & 31) != 0 || (aramAddr & 31) != 0 || (length & 31) != 0) {
        OSReport("AR: DMA addresses/length must be 32-byte aligned!\n");
        return;
    }
    
    // Verify ARAM bounds
    if (aramAddr + length > s_aramSize) {
        OSReport("AR: DMA would exceed ARAM bounds!\n");
        return;
    }
    
    s_dmaBusy = TRUE;
    
    // Perform DMA (instant memcpy on PC)
    void* aramPtr = (u8*)s_aramBase + aramAddr;
    void* mramPtr = (void*)mainmemAddr;
    
    if (type == AR_MRAM_TO_ARAM) {
        // Copy from main RAM to ARAM
        memcpy(aramPtr, mramPtr, length);
    } else {
        // Copy from ARAM to main RAM
        memcpy(mramPtr, aramPtr, length);
    }
    
    s_dmaBusy = FALSE;
    
    // Call callback if registered
    if (s_dmaCallback) {
        s_dmaCallback();
    }
}

/*---------------------------------------------------------------------------*
  Name:         ARGetDMAStatus

  Description:  Check if DMA operation is in progress.

  Arguments:    None

  Returns:      0 if idle, non-zero if busy
 *---------------------------------------------------------------------------*/
u32 ARGetDMAStatus(void) {
    return s_dmaBusy ? 1 : 0;
}

/*---------------------------------------------------------------------------*
  Name:         ARRegisterDMACallback

  Description:  Register callback to be called when DMA completes.

  Arguments:    callback  Callback function

  Returns:      Previous callback
 *---------------------------------------------------------------------------*/
ARCallback ARRegisterDMACallback(ARCallback callback) {
    ARCallback prev = s_dmaCallback;
    s_dmaCallback = callback;
    return prev;
}

/*---------------------------------------------------------------------------*
  Name:         __ARClearInterrupt

  Description:  Clear ARAM DMA interrupt flag.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void __ARClearInterrupt(void) {
    /* No hardware interrupts on PC */
}

/*---------------------------------------------------------------------------*
  Name:         __ARGetInterruptStatus

  Description:  Get ARAM interrupt status.

  Arguments:    None

  Returns:      Interrupt status (0 on PC)
 *---------------------------------------------------------------------------*/
u32 __ARGetInterruptStatus(void) {
    return 0;  // No interrupts on PC
}

/*---------------------------------------------------------------------------*
  Name:         ARSetSize

  Description:  Set ARAM size (obsolete - size is auto-detected).

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void ARSetSize(void) {
    /* Obsolete function - ARAM size is fixed at 16MB.
     * Exists for API compatibility only.
     */
}

