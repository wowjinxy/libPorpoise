/*---------------------------------------------------------------------------*
  OSContext.c - CPU Context Save/Restore
  
  On GC/Wii (PowerPC):
  - OSContext stores complete CPU register state (GPRs, FPRs, special regs)
  - Used for exception handling, thread switching, setjmp/longjmp
  - All implemented in assembly to directly access CPU registers
  - FPU context is saved lazily (only when needed)
  
  On PC (x86/x64/ARM):
  - Platform threads (Win32/pthread) handle their own contexts
  - We don't implement low-level exception handling
  - OS handles context switching automatically
  - Most functions are stubs or no-ops
  
  What we DO implement:
  - OSClearContext, OSInitContext - For API compatibility
  - OSGetCurrentContext, OSSetCurrentContext - Track active context
  - OSDumpContext - Debug output (useful!)
  
  What we DON'T need:
  - OSSaveContext, OSLoadContext - Platform threads handle this
  - FPU context switching - x86 handles FPU automatically
  - Stack switching - Platform threads have their own stacks
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static OSContext* s_currentContext = NULL;

/*---------------------------------------------------------------------------*
  Name:         OSGetStackPointer

  Description:  Returns the current stack pointer value.
                
                On PowerPC: Returns r1 register
                On PC: Not practical to get from C code reliably
                
                Most games don't actually use this. It's mainly for debugging.

  Returns:      Stack pointer (stub: returns 0 on PC)
 *---------------------------------------------------------------------------*/
u32 OSGetStackPointer(void) {
    /* On PC, each platform thread has its own stack managed by the OS.
     * We can't easily get the stack pointer from C code in a meaningful way.
     * Games rarely use this anyway - it's mainly for debugging.
     */
    return 0;
}

/*---------------------------------------------------------------------------*
  Name:         OSSwitchStack

  Description:  Switches to a new stack pointer and returns the old one.
                
                On PowerPC: Swaps r1 register
                On PC: Not safe to do from C code
                
                Games use this for coroutines or custom stack management.
                On PC, use platform threads instead.

  Arguments:    newsp - New stack pointer

  Returns:      Old stack pointer (stub: returns 0 on PC)
 *---------------------------------------------------------------------------*/
u32 OSSwitchStack(u32 newsp) {
    /* Stack switching is dangerous and not portable in C.
     * On PC, use proper threads instead of stack switching.
     * Games that use this will need to be refactored to use OSCreateThread.
     */
    (void)newsp;
    OSReport("WARNING: OSSwitchStack called - not supported on PC\n");
    OSReport("  Consider using OSCreateThread instead\n");
    return 0;
}

/*---------------------------------------------------------------------------*
  Name:         OSSwitchFiber

  Description:  Calls a function with a new stack, then restores old stack
                when it returns. This is like a coroutine switch.
                
                On PowerPC: Assembly routine that:
                1. Saves old SP
                2. Switches to new SP
                3. Calls function at 'pc'
                4. Restores old SP when function returns
                
                On PC: Not safe in C. Use threads or callbacks instead.

  Arguments:    pc    - Function to call
                newsp - Stack for the function

  Returns:      Return value from the function (stub: returns 0)
 *---------------------------------------------------------------------------*/
int OSSwitchFiber(u32 pc, u32 newsp) {
    /* Fiber switching requires assembly and is not portable.
     * Games using this are typically doing advanced coroutine tricks.
     * On PC, refactor to use callbacks or threads.
     */
    (void)pc;
    (void)newsp;
    OSReport("WARNING: OSSwitchFiber called - not supported on PC\n");
    OSReport("  Consider using callback pattern instead\n");
    return 0;
}

/*---------------------------------------------------------------------------*
  Name:         OSSwitchFiberEx

  Description:  Like OSSwitchFiber but can pass up to 4 arguments to the
                function.

  Arguments:    arg0-arg3 - Arguments to pass to function
                pc        - Function to call
                newsp     - Stack for the function

  Returns:      Return value from the function (stub: returns 0)
 *---------------------------------------------------------------------------*/
int OSSwitchFiberEx(u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 pc, u32 newsp) {
    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)pc;
    (void)newsp;
    OSReport("WARNING: OSSwitchFiberEx called - not supported on PC\n");
    return 0;
}

/*---------------------------------------------------------------------------*
  Name:         OSSetCurrentContext / OSGetCurrentContext

  Description:  Tracks which OSContext is currently active. Used by the
                exception system and thread scheduler.
                
                On original hardware, this is stored in low memory and used
                by interrupt handlers.
                
                On PC: We just track it in a global variable. Platform threads
                don't actually use this, but we maintain it for API compatibility.

  Arguments:    context - Context to make current (Set function)

  Returns:      Current context (Get function)
 *---------------------------------------------------------------------------*/
void OSSetCurrentContext(OSContext* context) {
    s_currentContext = context;
}

OSContext* OSGetCurrentContext(void) {
    return s_currentContext;
}

/*---------------------------------------------------------------------------*
  Name:         OSSaveContext

  Description:  Saves the current CPU register state to a context structure.
                Analogous to setjmp().
                
                On PowerPC: Assembly that saves all registers (GPRs, special regs)
                Returns 0 when called, returns 1 when restored via OSLoadContext
                
                On PC: Not implementable in portable C. Platform threads handle
                context switching. Games using setjmp/longjmp pattern should
                use actual setjmp/longjmp instead.

  Arguments:    context - Where to save state

  Returns:      0 when saving, 1 when restored (stub: always returns 0)
 *---------------------------------------------------------------------------*/
u32 OSSaveContext(OSContext* context) {
    /* This is PowerPC-specific assembly that saves r0-r31, LR, CR, etc.
     * On PC:
     * - Can't access CPU registers from C
     * - Platform threads handle their own context
     * - Games using this for setjmp/longjmp should use <setjmp.h> instead
     * 
     * We provide a stub that at least clears the context for safety.
     */
    if (context) {
        memset(context, 0, sizeof(OSContext));
    }
    
    OSReport("WARNING: OSSaveContext called - not fully supported on PC\n");
    OSReport("  For setjmp/longjmp behavior, use standard <setjmp.h> instead\n");
    return 0;
}

/*---------------------------------------------------------------------------*
  Name:         OSLoadContext

  Description:  Restores CPU state from a context. Analogous to longjmp().
                
                On PowerPC: Restores all registers and uses RFI (return from
                interrupt) to jump to the saved PC.
                
                On PC: Not implementable. Use actual longjmp() instead.

  Arguments:    context - Context to restore

  Returns:      Does not return (stub: just returns)
 *---------------------------------------------------------------------------*/
void OSLoadContext(OSContext* context) {
    /* This uses PowerPC RFI (Return From Interrupt) instruction to
     * restore registers and jump to saved PC.
     * 
     * On PC: Not possible in C. Games using this should refactor to
     * use standard longjmp() or callbacks.
     */
    (void)context;
    OSReport("WARNING: OSLoadContext called - not supported on PC\n");
    OSReport("  Consider using longjmp() or callback patterns\n");
}

/*---------------------------------------------------------------------------*
  Name:         OSClearContext

  Description:  Clears/initializes a context structure. Called before the
                context is set as current, or when context is no longer used.
                
                On PC: Simple memset. We can implement this properly.

  Arguments:    context - Context to clear

  Returns:      None
 *---------------------------------------------------------------------------*/
void OSClearContext(OSContext* context) {
    if (!context) return;
    
    /* Just zero out the structure */
    memset(context, 0, sizeof(OSContext));
    context->mode = 0;
    context->state = 0;
}

/*---------------------------------------------------------------------------*
  Name:         OSInitContext

  Description:  Initializes a context with a given program counter and stack
                pointer. Used to set up a context for a new thread or function.
                
                On PowerPC: Sets PC (SRR0), SP (r1), and initializes MSR with
                appropriate flags (interrupts enabled, address translation on).
                
                On PC: We can set the fields, but they won't actually be used
                by our platform threads. Still useful for API compatibility.

  Arguments:    context - Context to initialize
                pc      - Program counter (function address)
                sp      - Stack pointer (top of stack)

  Returns:      None
 *---------------------------------------------------------------------------*/
void OSInitContext(OSContext* context, u32 pc, u32 sp) {
    if (!context) return;
    
    /* Clear the context first */
    memset(context, 0, sizeof(OSContext));
    
    /* Set program counter (SRR0) */
    context->srr0 = pc;
    
    /* Set stack pointer (r1) */
    context->gpr[1] = sp;
    
    /* Set up MSR with reasonable defaults:
     * On PowerPC this would enable interrupts, address translation, etc.
     * On PC these values don't actually matter, but we set them for
     * compatibility if games read the context.
     */
    context->srr1 = 0x00009032; // MSR_EE | MSR_IR | MSR_DR | MSR_RI | MSR_ME
    
    /* Clear condition register, XER */
    context->cr = 0;
    context->xer = 0;
    
    /* Note: On original hardware, r2 (RTOC) and r13 (SDA) are set to
     * current values from CPU. On PC we just leave them as 0.
     */
}

/*---------------------------------------------------------------------------*
  Name:         OSLoadFPUContext / OSSaveFPUContext

  Description:  Loads/saves floating-point registers from/to context.
                
                On PowerPC: Assembly that loads/stores fp0-fp31, FPSCR, and
                paired singles (Gekko-specific SIMD registers).
                
                On PC: x86 FPU/SSE is completely different architecture.
                Platform threads handle their own FPU state.
                These are no-ops.

  Arguments:    context - Context with FPU state

  Returns:      None
 *---------------------------------------------------------------------------*/
void OSLoadFPUContext(OSContext* context) {
    /* PowerPC has 32 double-precision FPU registers (fp0-fp31).
     * x86/x64 has different FPU/SSE architecture.
     * Platform threads save/restore their own FPU state automatically.
     * This is a no-op on PC.
     */
    (void)context;
}

void OSSaveFPUContext(OSContext* context) {
    /* Same as OSLoadFPUContext - not needed on PC */
    (void)context;
}

/*---------------------------------------------------------------------------*
  Name:         OSFillFPUContext

  Description:  Saves current FPU register contents into a context.
                Used by exception handlers to capture FPU state.
                
                On PC: No-op since we don't implement exception handlers.

  Arguments:    context - Context to fill with FPU state

  Returns:      None
 *---------------------------------------------------------------------------*/
void OSFillFPUContext(OSContext* context) {
    /* Used to capture current FPU state into a context.
     * On PC, platform exception handlers do this automatically.
     */
    (void)context;
}

/*---------------------------------------------------------------------------*
  Name:         OSDumpContext

  Description:  Prints a context structure for debugging. Very useful for
                debugging crashes and exceptions.
                
                This is one of the few context functions that's actually
                useful on PC! We can implement it properly.

  Arguments:    context - Context to dump

  Returns:      None
 *---------------------------------------------------------------------------*/
void OSDumpContext(OSContext* context) {
    if (!context) {
        OSReport("OSDumpContext: NULL context\n");
        return;
    }
    
    OSReport("\n");
    OSReport("==================== Context Dump ====================\n");
    OSReport("Context at: %p\n", context);
    OSReport("\n");
    
    /* Program counter and stack pointer */
    OSReport("PC (SRR0) = 0x%08X\n", context->srr0);
    OSReport("SP (r1)   = 0x%08X\n", context->gpr[1]);
    OSReport("LR        = 0x%08X\n", context->lr);
    OSReport("MSR       = 0x%08X (SRR1)\n", context->srr1);
    OSReport("\n");
    
    /* General purpose registers (in pairs for readability) */
    OSReport("General Purpose Registers:\n");
    OSReport("-----------------------------------------------------\n");
    for (int i = 0; i < 16; i++) {
        OSReport("r%-2d = 0x%08X (%11d)  r%-2d = 0x%08X (%11d)\n",
                 i, context->gpr[i], (s32)context->gpr[i],
                 i + 16, context->gpr[i + 16], (s32)context->gpr[i + 16]);
    }
    OSReport("\n");
    
    /* Special registers */
    OSReport("Special Registers:\n");
    OSReport("-----------------------------------------------------\n");
    OSReport("CR  (Condition)    = 0x%08X\n", context->cr);
    OSReport("CTR (Count)        = 0x%08X\n", context->ctr);
    OSReport("XER (Fixed-Point)  = 0x%08X\n", context->xer);
    OSReport("FPSCR (FP Status)  = 0x%08X\n", context->fpscr);
    OSReport("\n");
    
    /* Context state */
    OSReport("Context State:\n");
    OSReport("-----------------------------------------------------\n");
    OSReport("Mode  = 0x%04X ", context->mode);
    if (context->mode & 0x01) OSReport("[FPU] ");
    if (context->mode & 0x02) OSReport("[PSFP] ");
    OSReport("\n");
    
    OSReport("State = 0x%04X ", context->state);
    if (context->state & 0x01) OSReport("[FPSAVED] ");
    if (context->state & 0x02) OSReport("[EXC] ");
    OSReport("\n");
    OSReport("\n");
    
    /* GQR registers (Gekko graphics quantization - usually all zero on PC) */
    BOOL has_gqr = FALSE;
    for (int i = 0; i < 8; i++) {
        if (context->gqr[i] != 0) {
            has_gqr = TRUE;
            break;
        }
    }
    
    if (has_gqr) {
        OSReport("Graphics Quantization Registers (GQR):\n");
        OSReport("-----------------------------------------------------\n");
        for (int i = 0; i < 4; i++) {
            OSReport("GQR%d = 0x%08X  GQR%d = 0x%08X\n",
                     i, context->gqr[i], i + 4, context->gqr[i + 4]);
        }
        OSReport("\n");
    }
    
    /* Floating-point registers (if saved) */
    if (context->state & 0x01) { // OS_CONTEXT_STATE_FPSAVED
        OSReport("Floating-Point Registers:\n");
        OSReport("-----------------------------------------------------\n");
        for (int i = 0; i < 8; i++) {
            OSReport("fp%-2d = %.6f  fp%-2d = %.6f  fp%-2d = %.6f  fp%-2d = %.6f\n",
                     i*4, context->fpr[i*4],
                     i*4+1, context->fpr[i*4+1],
                     i*4+2, context->fpr[i*4+2],
                     i*4+3, context->fpr[i*4+3]);
        }
        OSReport("\n");
    }
    
    /* Stack crawl (if we have a valid stack pointer) */
    if (context->gpr[1] != 0 && context->gpr[1] != 0xFFFFFFFF) {
        OSReport("Stack Backtrace (simulated):\n");
        OSReport("-----------------------------------------------------\n");
        OSReport("SP: 0x%08X, LR: 0x%08X\n", context->gpr[1], context->lr);
        OSReport("(Note: Full stack crawl not available on PC)\n");
        OSReport("\n");
    }
    
    OSReport("======================================================\n");
    OSReport("\n");
}

/*---------------------------------------------------------------------------*
  Name:         OSSetCurrentContext

  Description:  Sets the current context pointer. On original hardware, this
                also manages the FPU enable bit and updates low memory.
                
                On PC: We just track the pointer. Platform threads manage
                their own contexts.

  Arguments:    context - Context to make current

  Returns:      None
 *---------------------------------------------------------------------------*/
void OSSetCurrentContext(OSContext* context) {
    s_currentContext = context;
}

/*---------------------------------------------------------------------------*
  Name:         OSGetCurrentContext

  Description:  Returns pointer to the current context.

  Returns:      Current context pointer, or NULL if none set
 *---------------------------------------------------------------------------*/
OSContext* OSGetCurrentContext(void) {
    return s_currentContext;
}

/*---------------------------------------------------------------------------*
  Name:         OSSaveContext

  Description:  Saves current CPU state to context. Like setjmp().
                
                On PowerPC: Assembly that saves all 32 GPRs, special registers,
                and returns 0. When restored via OSLoadContext, returns 1.
                
                On PC: Not implementable in portable C. Use <setjmp.h> instead.

  Arguments:    context - Where to save CPU state

  Returns:      0 when saving, 1 when restored (stub: always 0)
 *---------------------------------------------------------------------------*/
u32 OSSaveContext(OSContext* context) {
    if (!context) return 0;
    
    /* Clear context for safety */
    OSClearContext(context);
    
    /* We can't actually save CPU registers from C code.
     * Games using OSSaveContext/OSLoadContext should be refactored to use:
     * 
     * #include <setjmp.h>
     * jmp_buf buf;
     * if (setjmp(buf) == 0) {
     *     // First call
     * } else {
     *     // Returned via longjmp
     * }
     */
    
    return 0;
}

/*---------------------------------------------------------------------------*
  Name:         OSLoadContext

  Description:  Restores CPU state from context. Like longjmp().
                
                On PowerPC: Uses RFI (Return From Interrupt) to restore
                registers and jump to saved PC. Does not return.
                
                On PC: Not implementable. Use longjmp() instead.

  Arguments:    context - Context to restore

  Returns:      Does not return (stub: just returns)
 *---------------------------------------------------------------------------*/
void OSLoadContext(OSContext* context) {
    (void)context;
    
    /* Cannot implement in C - requires direct CPU register access.
     * Games should use longjmp() instead.
     */
}

/*---------------------------------------------------------------------------*
  Name:         OSClearContext

  Description:  Clears a context structure. Must be called before using a
                context, or when the context is no longer needed.
                
                On PC: Simple and works correctly.

  Arguments:    context - Context to clear

  Returns:      None
 *---------------------------------------------------------------------------*/
void OSClearContext(OSContext* context) {
    if (!context) return;
    
    memset(context, 0, sizeof(OSContext));
    context->mode = 0;
    context->state = 0;
}

/*---------------------------------------------------------------------------*
  Name:         OSInitContext

  Description:  Initializes a context with starting PC and stack pointer.
                Used to set up contexts for new threads.
                
                On PowerPC: Sets up all necessary registers, MSR flags, etc.
                
                On PC: We set the fields even though platform threads don't
                use them. Useful for debugging and API compatibility.

  Arguments:    context - Context to initialize
                pc      - Starting program counter (function address)
                sp      - Stack pointer (top of stack)

  Returns:      None
 *---------------------------------------------------------------------------*/
void OSInitContext(OSContext* context, u32 pc, u32 sp) {
    if (!context) return;
    
    /* Clear everything first */
    memset(context, 0, sizeof(OSContext));
    
    /* Set program counter */
    context->srr0 = pc;
    
    /* Set stack pointer (r1) */
    context->gpr[1] = sp;
    
    /* Set up machine state register with sensible defaults:
     * MSR_EE (0x8000) - External interrupts enabled
     * MSR_IR (0x0020) - Instruction address translation on
     * MSR_DR (0x0010) - Data address translation on
     * MSR_RI (0x0002) - Recoverable interrupt
     * MSR_ME (0x1000) - Machine check enabled
     * = 0x00009032
     */
    context->srr1 = 0x00009032;
    
    /* Clear condition register and XER */
    context->cr = 0;
    context->xer = 0;
    context->ctr = 0;
    context->lr = 0;
    
    /* Initialize mode flags */
    context->mode = 0;
    context->state = 0;
    
    /* GQRs are Gekko-specific, leave as 0 */
    /* FPRs are cleared by memset */
}
