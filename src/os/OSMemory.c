/*---------------------------------------------------------------------------*
  OSMemory.c - Memory Sizing and Protection
  
  HARDWARE MEMORY SYSTEM (GC/Wii):
  =================================
  
  The original GameCube/Wii had specific memory configurations:
  
  **GameCube (Gekko CPU + Flipper):**
  - **MEM1 (Main Memory):** 24 MB ARAM (1T-SRAM)
    * Fast, low-latency
    * Addresses: 0x80000000-0x817FFFFF (cached)
    * Addresses: 0xC0000000-0xC17FFFFF (uncached)
  
  **Wii (Broadway CPU + Hollywood):**
  - **MEM1 (Main Memory):** 24 MB (compatibility with GC)
  - **MEM2 (Extended Memory):** 64 MB GDDR3
    * Slower than MEM1, but much larger
    * Addresses: 0x90000000-0x93FFFFFF (cached)
    * Addresses: 0xD0000000-0xD3FFFFFF (uncached)
  
  **BAT Register Configuration:**
  - PowerPC BAT (Block Address Translation) registers map virtual to physical
  - BAT0/BAT2: Map MEM1 (24MB in multiple blocks)
  - BAT4/BAT5: Map MEM2 (64MB for Wii)
  - Configured during boot by IPL
  
  **Memory Protection Hardware:**
  - Flipper/Hollywood MEM controller has 4 protection channels
  - Each channel can protect a memory range (1KB granularity)
  - Protection types: NONE, READ-only, WRITE-only, READ/WRITE (full protect)
  - Generates MEM interrupt on violation
  - Hardware registers:
    * MEM_MARR0_START/END - Protection channel 0 range
    * MEM_MARR1_START/END - Protection channel 1 range
    * MEM_MARR2_START/END - Protection channel 2 range
    * MEM_MARR3_START/END - Protection channel 3 range
    * MEM_MARR_CONTROL - Control register (read/write bits)
    * MEM_INT_STAT - Interrupt status (which channel triggered)
    * MEM_INT_ADDRH/ADDRL - Address that caused violation
  
  **Typical Game Usage:**
  ```c
  // Protect code section from being overwritten
  extern u8 _text_start[];
  extern u8 _text_end[];
  OSProtectRange(OS_PROTECT_CHAN0, _text_start, 
                 _text_end - _text_start,
                 OS_PROTECT_CONTROL_READ);  // Read-only
  
  // MEM interrupt triggers on write attempt to protected range
  ```
  
  PC PORT REALITY:
  ================
  
  **Memory Sizing - SIMPLE:**
  ✅ Just return fixed sizes (24MB + 64MB)
  ✅ Games use this to know how much memory is available
  ✅ On PC, we have way more memory, but we simulate the limits
  ✅ OSAlloc uses these values to create arenas
  
  **Memory Protection - COMPLEX:**
  
  Why We Can't Port It Directly:
  
  1. **No Hardware Registers** ❌
     - No MEM_MARR registers on PC
     - Can't configure protection at hardware level
     - x86/ARM MMU works differently (page-based)
  
  2. **Page Granularity** ❌
     - PowerPC: 1KB granularity (flexible)
     - x86/x64: 4KB page granularity (coarse)
     - Can't protect arbitrary 1KB blocks
  
  3. **OS-Level Protection** ❌
     - Windows: VirtualProtect() (PAGE_READONLY, PAGE_READWRITE, etc.)
     - Linux: mprotect() (PROT_READ, PROT_WRITE, PROT_EXEC)
     - Requires page alignment (4KB boundaries)
     - May not work with heap allocations
  
  4. **Exception Handling** ❌
     - GC/Wii: MEM interrupt → MEMInterruptHandler()
     - PC: Access violation → OS exception handler
     - Hard to intercept and map to OSError system
  
  **What We CAN Do:**
  
  Option 1: **No-Op (Current Implementation)** ✅
  - OSProtectRange does nothing
  - Games that use it for debugging won't break
  - No actual protection (but PC has OS-level protection anyway)
  
  Option 2: **Platform Protection (Advanced)** ⚠️
  - Use VirtualProtect (Windows) / mprotect (Linux)
  - Round addresses to page boundaries
  - Set up exception handler to catch violations
  - Map to OS_ERROR_PROTECTION
  - Complex, platform-specific, may not work with all allocators
  
  Option 3: **Debug Mode Only** 🔍
  - In debug builds, track protected ranges
  - Insert checks in memory access wrappers
  - Report violations (without hardware support)
  - Zero overhead in release builds
  
  **Current Implementation:**
  We use Option 1 (No-Op) because:
  - Most games don't rely on memory protection
  - Used mostly for debugging during development
  - OS-level protection (DEP, ASLR) provides security on PC
  - Simpler and more compatible
  
  MIGRATION NOTES:
  ================
  
  **If your game uses OSProtectRange:**
  
  1. **For Debugging** (most common):
     - Remove OSProtectRange calls
     - Use debugger memory watchpoints instead
     - PC debuggers have better tools (Visual Studio, GDB)
  
  2. **For Security** (rare):
     - Rely on OS-level protection (DEP, W^X)
     - Modern OSes prevent code modification by default
     - Don't need explicit protection
  
  3. **For ROM Protection** (very rare):
     - If protecting read-only data, use const keyword
     - Compiler/OS will place in read-only section
     - Hardware enforces it automatically
  
  4. **For Heap Protection** (advanced):
     - Use platform APIs (VirtualProtect/mprotect)
     - Implement custom wrapper if needed
     - See FUTURE_MEMORY_PROTECTION.md (if created)
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <string.h>

/* Simulated memory sizes (matches real hardware) */
#define SIMULATED_MEM1_SIZE  (24 * 1024 * 1024)  /* 24 MB */
#define SIMULATED_MEM2_SIZE  (64 * 1024 * 1024)  /* 64 MB */

/* Protection state tracking (for API compatibility and debug logging) */
#ifdef _DEBUG
typedef struct {
    void* addr;
    u32   size;
    u32   control;
    BOOL  active;
} ProtectionChannel;

static ProtectionChannel s_protectionChannels[4] = {0};
#endif

/*---------------------------------------------------------------------------*
  Name:         OSGetPhysicalMem1Size

  Description:  Returns the size of MEM1 (main memory).
                
                On original hardware: Reads from low memory location set by IPL
                (Initial Program Loader) during boot. Value comes from hardware.
                
                On PC: Returns simulated size (24 MB like real hardware).

  Returns:      MEM1 size in bytes (24 MB)
 *---------------------------------------------------------------------------*/
u32 OSGetPhysicalMem1Size(void) {
    /* Original hardware:
     * - IPL detects memory size during boot
     * - Stores at OS_NAPA_SIZE_PHYSICAL (low memory)
     * - Value: 24MB for all GameCube/Wii consoles
     * 
     * PC: Return fixed 24MB to match hardware.
     */
    return SIMULATED_MEM1_SIZE;
}

/*---------------------------------------------------------------------------*
  Name:         OSGetPhysicalMem2Size

  Description:  Returns the size of MEM2 (extended memory, Wii only).
                
                On original hardware: Reads from low memory location.
                Returns 0 on GameCube (no MEM2).
                
                On PC: Returns simulated size (64 MB like Wii).

  Returns:      MEM2 size in bytes (64 MB for Wii mode, 0 for GC mode)
 *---------------------------------------------------------------------------*/
u32 OSGetPhysicalMem2Size(void) {
    /* Original hardware:
     * - IPL detects MEM2 (GDDR3) during boot (Wii only)
     * - Stores at OS_GDDR_SIZE_PHYSICAL
     * - Value: 64MB for Wii, 0 for GameCube
     * 
     * PC: Return 64MB for Wii emulation mode.
     * If you want GameCube mode, return 0 here.
     */
    return SIMULATED_MEM2_SIZE;
}

/*---------------------------------------------------------------------------*
  Name:         OSGetConsoleSimulatedMem1Size

  Description:  Returns "simulated" MEM1 size. On dev kits, this could be
                different from physical size (for testing low-memory scenarios).
                
                On original hardware: Reads from OS_NAPA_SIZE_SIMULATED.
                Usually same as physical size on retail.
                
                On PC: Same as physical size.

  Returns:      Simulated MEM1 size in bytes (24 MB)
 *---------------------------------------------------------------------------*/
u32 OSGetConsoleSimulatedMem1Size(void) {
    /* Original hardware:
     * - Dev kits can simulate lower memory
     * - Helps test game behavior with limited RAM
     * - OS_NAPA_SIZE_SIMULATED can be < physical
     * 
     * PC: No need for simulation, return same as physical.
     */
    return OSGetPhysicalMem1Size();
}

/*---------------------------------------------------------------------------*
  Name:         OSGetConsoleSimulatedMem2Size

  Description:  Returns "simulated" MEM2 size (for dev kit testing).

  Returns:      Simulated MEM2 size in bytes (64 MB)
 *---------------------------------------------------------------------------*/
u32 OSGetConsoleSimulatedMem2Size(void) {
    /* Same reasoning as MEM1 simulated size. */
    return OSGetPhysicalMem2Size();
}

/*===========================================================================*
  MEMORY PROTECTION
  
  OSProtectRange() is a hardware-based memory protection system on GC/Wii.
  On PC, we can't replicate it exactly, so we provide a compatible stub.
 *===========================================================================*/

/*---------------------------------------------------------------------------*
  Name:         OSProtectRange

  Description:  Sets up memory protection on a range of addresses.
                
                On original hardware:
                - Configures MEM controller protection channels
                - 4 channels available (OS_PROTECT_CHAN0-3)
                - Each protects one memory range
                - 1KB granularity (addr & size rounded to 1KB)
                - Protection types:
                  * OS_PROTECT_CONTROL_NONE  - No protection
                  * OS_PROTECT_CONTROL_READ  - Read-only
                  * OS_PROTECT_CONTROL_WRITE - Write-only
                  * OS_PROTECT_CONTROL_RDWR  - Full protect
                - Violation triggers MEM interrupt → OS_ERROR_PROTECTION
                
                On PC: STUB - No actual protection applied.
                
                Why not implemented:
                - No MEM hardware controller on PC
                - OS-level protection (VirtualProtect/mprotect) has different
                  granularity (4KB pages vs 1KB)
                - Most games use this for debugging, not critical functionality
                - PC OSes provide their own memory protection (DEP, etc.)

  Arguments:    chan    - Protection channel (0-3)
                addr    - Start address (rounded down to 1KB boundary)
                nBytes  - Size (addr+nBytes rounded up to 1KB boundary)
                control - Protection flags (OS_PROTECT_CONTROL_*)

  Returns:      None
  
  Migration:
  - For debugging: Use debugger memory watchpoints
  - For security: Rely on OS protection (DEP, W^X)
  - For ROM data: Use 'const' keyword
 *---------------------------------------------------------------------------*/
void OSProtectRange(u32 chan, void* addr, u32 nBytes, u32 control) {
    /* Validate parameters (same as original) */
    if (chan >= 4) {
        return;
    }
    
    /* Original hardware:
     * 1. Flush data cache for the range (DCFlushRange)
     * 2. Disable interrupts (OSDisableInterrupts)
     * 3. Mask MEM interrupt for this channel
     * 4. Configure MEM_MARR registers:
     *    - MEM_MARR0_START/END for channel 0
     *    - MEM_MARR1_START/END for channel 1
     *    - MEM_MARR2_START/END for channel 2
     *    - MEM_MARR3_START/END for channel 3
     * 5. Set MEM_MARR_CONTROL with protection bits
     * 6. Unmask MEM interrupt for this channel
     * 7. Restore interrupts
     * 
     * PC: Can't configure hardware. Just track for debugging.
     */
    
#ifdef _DEBUG
    /* Debug mode: Track protection ranges for logging/validation */
    u32 start = (u32)addr & ~0x3FF;  /* Round down to 1KB */
    u32 end = ((u32)addr + nBytes + 0x3FF) & ~0x3FF;  /* Round up to 1KB */
    
    s_protectionChannels[chan].addr = (void*)start;
    s_protectionChannels[chan].size = end - start;
    s_protectionChannels[chan].control = control & OS_PROTECT_CONTROL_RDWR;
    s_protectionChannels[chan].active = (control != OS_PROTECT_CONTROL_NONE);
    
    const char* controlStr = "NONE";
    if (control == OS_PROTECT_CONTROL_READ) {
        controlStr = "READ-ONLY";
    } else if (control == OS_PROTECT_CONTROL_WRITE) {
        controlStr = "WRITE-ONLY";
    } else if (control == OS_PROTECT_CONTROL_RDWR) {
        controlStr = "READ/WRITE (FULL)";
    }
    
    if (s_protectionChannels[chan].active) {
        OSReport("[OSMemory] Channel %u: Protect 0x%08X - 0x%08X (%u bytes) [%s]\n",
                 chan, start, end, end - start, controlStr);
        OSReport("            NOTE: Protection is NOT enforced on PC!\n");
        OSReport("            Use debugger watchpoints for memory debugging.\n");
    } else {
        OSReport("[OSMemory] Channel %u: Protection disabled\n", chan);
    }
#else
    /* Release mode: No-op (zero overhead) */
    (void)chan;
    (void)addr;
    (void)nBytes;
    (void)control;
#endif
    
    /* TODO (Advanced): Platform-specific memory protection
     * 
     * Windows example:
     * DWORD oldProtect;
     * VirtualProtect(addr, nBytes, PAGE_READONLY, &oldProtect);
     * 
     * Linux example:
     * mprotect(addr, nBytes, PROT_READ);
     * 
     * Issues:
     * - Requires page alignment (4KB on x86/x64)
     * - May not work with heap allocations
     * - Needs exception handler to map violations to OS_ERROR_PROTECTION
     * - Platform-specific code
     * 
     * For now, we leave it as a no-op for maximum compatibility.
     */
}

/*---------------------------------------------------------------------------*
  Name:         __OSMemoryInit (Internal, not in original API)

  Description:  Initializes memory system. Called by OSInit().
                
                On original hardware:
                - Sets up MEM interrupt handler (MEMInterruptHandler)
                - Configures default protection state
                - Registers shutdown function to disable protection
                
                On PC: Just initializes our tracking structures.

  Arguments:    None
 *---------------------------------------------------------------------------*/
void __OSMemoryInit(void) {
#ifdef _DEBUG
    /* Clear protection channels */
    memset(s_protectionChannels, 0, sizeof(s_protectionChannels));
    
    OSReport("[OSMemory] Initialized\n");
    OSReport("           MEM1: %u MB (%u bytes)\n",
             SIMULATED_MEM1_SIZE / (1024*1024), SIMULATED_MEM1_SIZE);
    OSReport("           MEM2: %u MB (%u bytes)\n",
             SIMULATED_MEM2_SIZE / (1024*1024), SIMULATED_MEM2_SIZE);
#endif
    
    /* Original hardware also:
     * - Registers MEMInterruptHandler for MEM_0 through MEM_3 interrupts
     * - Masks all MEM interrupts initially
     * - Registers shutdown function to disable protection on reset
     * 
     * PC: No interrupts to register, no shutdown needed.
     */
}

/*---------------------------------------------------------------------------*
  FUTURE ENHANCEMENT: Platform Memory Protection
  
  If you need real memory protection on PC, you could implement:
  
  Windows (VirtualProtect):
  -------------------------
  DWORD FlipperToPlatformProtection(u32 control) {
      if (control == OS_PROTECT_CONTROL_NONE) return PAGE_READWRITE;
      if (control == OS_PROTECT_CONTROL_READ) return PAGE_READONLY;
      if (control == OS_PROTECT_CONTROL_WRITE) return PAGE_WRITECOPY;
      if (control == OS_PROTECT_CONTROL_RDWR) return PAGE_NOACCESS;
      return PAGE_READWRITE;
  }
  
  void OSProtectRange(u32 chan, void* addr, u32 nBytes, u32 control) {
      // Round to page boundaries (4KB)
      void* pageAddr = (void*)((uintptr_t)addr & ~0xFFF);
      size_t pageSize = (nBytes + 0xFFF) & ~0xFFF;
      
      DWORD protect = FlipperToPlatformProtection(control);
      DWORD oldProtect;
      
      if (!VirtualProtect(pageAddr, pageSize, protect, &oldProtect)) {
          OSReport("VirtualProtect failed: %u\n", GetLastError());
      }
  }
  
  Linux (mprotect):
  -----------------
  int FlipperToPlatformProtection(u32 control) {
      if (control == OS_PROTECT_CONTROL_NONE) return PROT_READ | PROT_WRITE;
      if (control == OS_PROTECT_CONTROL_READ) return PROT_READ;
      if (control == OS_PROTECT_CONTROL_WRITE) return PROT_WRITE;
      if (control == OS_PROTECT_CONTROL_RDWR) return PROT_NONE;
      return PROT_READ | PROT_WRITE;
  }
  
  void OSProtectRange(u32 chan, void* addr, u32 nBytes, u32 control) {
      // Round to page boundaries
      void* pageAddr = (void*)((uintptr_t)addr & ~0xFFF);
      size_t pageSize = (nBytes + 0xFFF) & ~0xFFF;
      
      int prot = FlipperToPlatformProtection(control);
      
      if (mprotect(pageAddr, pageSize, prot) != 0) {
          OSReport("mprotect failed: %s\n", strerror(errno));
      }
  }
  
  Exception Handling:
  -------------------
  You'd also need to set up platform exception handlers to catch
  access violations and map them to OS_ERROR_PROTECTION.
  See OSError.c for details on exception handling integration.
 *---------------------------------------------------------------------------*/
