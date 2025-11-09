/*---------------------------------------------------------------------------*
  OSError.c - Exception and Error Handling
  
  On GC/Wii (PowerPC):
  - Handles CPU exceptions (machine check, DSI, alignment, etc.)
  - Exception handlers are called from first-level exception vectors
  - Can install custom handlers for each exception type
  - FPU exceptions can be enabled/disabled per thread
  
  On PC (x86/x64/ARM):
  - Modern OS handles exceptions (segfault, divide by zero, etc.)
  - We can't intercept hardware exceptions from user code
  - No low-level exception system to hook into
  
  What we DO provide:
  - OSSetErrorHandler() - Track handlers for API compatibility
  - Error type constants - For code that checks them
  - __OSFpscrEnableBits - FPU exception mask
  - __OSUnhandledException() - Debug crash reporting
  
  What we DON'T implement (yet):
  - Actual exception interception - OS handles it
  
  FUTURE ENHANCEMENT: Signal/SEH Integration
  ==========================================
  We could intercept platform exceptions and call user error handlers:
  
  Windows (SEH):
  --------------
  - Use __try/__except blocks or SetUnhandledExceptionFilter()
  - Catch: EXCEPTION_ACCESS_VIOLATION -> OS_ERROR_DSI
           EXCEPTION_INT_DIVIDE_BY_ZERO -> OS_ERROR_PROGRAM
           EXCEPTION_FLT_* -> OS_ERROR_FPE
  - Convert EXCEPTION_POINTERS to OSContext
  - Call registered error handler
  - Allow EXCEPTION_CONTINUE_EXECUTION if handler fixes it
  
  Linux/POSIX (Signals):
  ----------------------
  - Use sigaction() to catch: SIGSEGV -> OS_ERROR_DSI
                              SIGFPE -> OS_ERROR_FPE
                              SIGILL -> OS_ERROR_PROGRAM
                              SIGBUS -> OS_ERROR_ALIGNMENT
  - Convert sigcontext to OSContext
  - Call registered error handler
  - Use siglongjmp if handler wants to recover
  
  Benefits:
  - Games with custom error handlers would work
  - Better crash reporting
  - Potential for error recovery (rare but possible)
  
  Challenges:
  - Platform-specific code (Win32 vs POSIX)
  - Context conversion (x86 regs -> PowerPC-style OSContext)
  - Recovery is tricky (longjmp from signal handler)
  - May interfere with debuggers
  
  For now: Not implemented. Most games don't need it.
  Note: Games rarely install custom exception handlers anyway. Most just
  let the default handler (which calls OSPanic) do its job.
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Exception error handler table */
static OSErrorHandler s_errorHandlers[OS_ERROR_MAX] = {NULL};

/* FPSCR enable bits - controls which floating-point exceptions trigger */
u32 __OSFpscrEnableBits = 0;

/* Exception names for debugging */
static const char* s_errorNames[] = {
    "System Reset",          // OS_ERROR_SYSTEM_RESET
    "Machine Check",         // OS_ERROR_MACHINE_CHECK
    "DSI",                   // OS_ERROR_DSI
    "ISI",                   // OS_ERROR_ISI
    "External Interrupt",    // OS_ERROR_EXTERNAL_INTERRUPT
    "Alignment",             // OS_ERROR_ALIGNMENT
    "Program",               // OS_ERROR_PROGRAM
    "Floating Point",        // OS_ERROR_FLOATING_POINT
    "Decrementer",           // OS_ERROR_DECREMENTER
    "System Call",           // OS_ERROR_SYSTEM_CALL
    "Trace",                 // OS_ERROR_TRACE
    "Performance Monitor",   // OS_ERROR_PERFORMACE_MONITOR
    "Breakpoint",            // OS_ERROR_BREAKPOINT
    "System Interrupt",      // OS_ERROR_SYSTEM_INTERRUPT
    "Thermal Interrupt",     // OS_ERROR_THERMAL_INTERRUPT
    "Protection",            // OS_ERROR_PROTECTION
    "FP Exception"           // OS_ERROR_FPE
};

/*---------------------------------------------------------------------------*
  Name:         OSSetErrorHandler

  Description:  Installs a custom error handler for a specific exception type.
                
                On original hardware:
                - Handler is called when that exception occurs
                - Can inspect context and decide what to do
                - Can modify context and return (recover)
                - Or can call OSPanic to crash
                
                On PC:
                - We track the handlers but can't actually call them
                - Modern OS handles exceptions via signals/SEH
                - Could potentially use signal handlers (SIGSEGV, etc.)
                
                Most games don't install custom handlers, they just let
                the default handler crash with OSPanic.

  Arguments:    error   - Error number (OS_ERROR_DSI, OS_ERROR_ALIGNMENT, etc.)
                handler - Handler function to call

  Returns:      Previous handler, or NULL if none was set
 *---------------------------------------------------------------------------*/
OSErrorHandler OSSetErrorHandler(OSError error, OSErrorHandler handler) {
    if (error >= OS_ERROR_MAX) {
        OSReport("OSSetErrorHandler: Invalid error %d (max is %d)\n", 
                 error, OS_ERROR_MAX - 1);
        return NULL;
    }
    
    OSErrorHandler old = s_errorHandlers[error];
    s_errorHandlers[error] = handler;
    
    /* Log handler installation for debugging */
    if (handler != NULL) {
        OSReport("Error handler installed for: %s (error %d)\n", 
                 s_errorNames[error], error);
    } else {
        OSReport("Error handler removed for: %s (error %d)\n",
                 s_errorNames[error], error);
    }
    
    /* Special handling for FPE (Floating-Point Exception) */
    if (error == OS_ERROR_FPE) {
        if (handler) {
            /* Enable floating-point exception generation
             * On PowerPC, this sets FPSCR enable bits for all threads
             * On PC, FPU exceptions are handled by the OS
             */
            OSReport("  Note: FPU exceptions enabled (not fully supported on PC)\n");
            OSReport("  Platform OS will handle divide-by-zero, invalid ops, etc.\n");
        } else {
            /* Disable FP exceptions */
            OSReport("  Note: FPU exceptions disabled\n");
        }
    }
    
    return old;
}

/*---------------------------------------------------------------------------*
  Name:         __OSUnhandledException (Internal)

  Description:  Called by the default exception handler when an exception
                occurs with no user handler, or after a user handler returns.
                
                On original hardware:
                - Called from assembly exception vectors
                - Dumps context and crash info
                - Calls PPCHalt() to freeze the system
                
                On PC:
                - We can't actually intercept exceptions from C
                - This is provided as a debug aid if called manually
                - Shows what the crash would look like on real hardware

  Arguments:    exception - Exception number
                context   - CPU context at time of exception
                dsisr     - DSI Status Register (memory fault info)
                dar       - Data Address Register (faulting address)

  Returns:      Does not return (halts system)
 *---------------------------------------------------------------------------*/
void __OSUnhandledException(__OSException exception, OSContext* context,
                            u32 dsisr, u32 dar) {
    OSTime now = OSGetTime();
    
    OSReport("\n");
    OSReport("====================================================\n");
    OSReport("           UNHANDLED EXCEPTION\n");
    OSReport("====================================================\n");
    
    /* Check if exception is recoverable */
    if (context && !(context->srr1 & 0x0002)) { // MSR_RI bit
        OSReport("Non-recoverable Exception %d", exception);
    } else {
        OSReport("Exception %d", exception);
    }
    
    if (exception < OS_ERROR_MAX) {
        OSReport(" (%s)\n", s_errorNames[exception]);
    } else {
        OSReport(" (Unknown)\n");
    }
    
    OSReport("----------------------------------------------------\n");
    
    /* Check if there's a user handler */
    if (exception < OS_ERROR_MAX && s_errorHandlers[exception]) {
        OSReport("User error handler is set but exception still unhandled\n");
        OSReport("Handler address: %p\n", s_errorHandlers[exception]);
        OSReport("\n");
        
        /* On PC, we could try calling the handler for debugging,
         * but it expects PowerPC context which we don't have */
    }
    
    /* Dump context */
    if (context) {
        OSDumpContext(context);
    }
    
    /* Additional info based on exception type */
    OSReport("\nException-Specific Information:\n");
    OSReport("----------------------------------------------------\n");
    OSReport("DSISR = 0x%08X  DAR = 0x%08X\n", dsisr, dar);
    OSReport("Time  = 0x%016llX\n", now);
    OSReport("\n");
    
    switch (exception) {
        case OS_ERROR_DSI:
            OSReport("DSI (Data Storage Interrupt):\n");
            OSReport("  Instruction at 0x%08X attempted to access\n", 
                     context ? context->srr0 : 0);
            OSReport("  invalid address 0x%08X\n", dar);
            OSReport("  This is like a SEGFAULT on Unix or Access Violation on Windows\n");
            break;
            
        case OS_ERROR_ISI:
            OSReport("ISI (Instruction Storage Interrupt):\n");
            OSReport("  Attempted to fetch instruction from invalid address 0x%08X\n",
                     context ? context->srr0 : 0);
            OSReport("  Execution jumped to unmapped memory\n");
            break;
            
        case OS_ERROR_ALIGNMENT:
            OSReport("Alignment Exception:\n");
            OSReport("  Instruction at 0x%08X attempted unaligned access\n",
                     context ? context->srr0 : 0);
            OSReport("  at address 0x%08X\n", dar);
            OSReport("  PowerPC requires aligned access (2-byte for u16, 4-byte for u32)\n");
            break;
            
        case OS_ERROR_PROGRAM:
            OSReport("Program Exception:\n");
            OSReport("  Possible illegal instruction or operation\n");
            OSReport("  at or around 0x%08X\n", context ? context->srr0 : 0);
            OSReport("  Could be: division by zero, privileged instruction,\n");
            OSReport("  invalid opcode, or trap instruction\n");
            break;
            
        case OS_ERROR_FPE:
            OSReport("Floating-Point Exception:\n");
            OSReport("  FPU exception occurred (overflow, underflow, etc.)\n");
            OSReport("  at 0x%08X\n", context ? context->srr0 : 0);
            break;
            
        case OS_ERROR_DECREMENTER:
            OSReport("Decrementer Exception:\n");
            OSReport("  Timer interrupt fired (used by OSAlarm system)\n");
            OSReport("  This should normally be handled, not crash\n");
            break;
            
        case OS_ERROR_PROTECTION:
            OSReport("Memory Protection Violation:\n");
            OSReport("  Access to protected memory region\n");
            break;
            
        default:
            OSReport("(No additional information for this exception type)\n");
            break;
    }
    
    OSReport("\n");
    OSReport("====================================================\n");
    OSReport("System Halted - Cannot Continue\n");
    OSReport("====================================================\n");
    
    /* On PC, we can't actually halt the CPU, so we abort */
    abort();
}

/*---------------------------------------------------------------------------*
  Name:         __OSGetErrorHandler (Internal)

  Description:  Gets the currently installed handler for an error type.
                Used internally by exception dispatch code.

  Arguments:    error - Error number

  Returns:      Handler function pointer, or NULL
 *---------------------------------------------------------------------------*/
OSErrorHandler __OSGetErrorHandler(OSError error) {
    if (error >= OS_ERROR_MAX) {
        return NULL;
    }
    return s_errorHandlers[error];
}

/*---------------------------------------------------------------------------*
  Name:         OSGetErrorName (Utility)

  Description:  Returns human-readable name for an error type.
                Not in original SDK but useful for debugging on PC.

  Arguments:    error - Error number

  Returns:      Error name string
 *---------------------------------------------------------------------------*/
const char* OSGetErrorName(OSError error) {
    if (error < OS_ERROR_MAX) {
        return s_errorNames[error];
    }
    return "Unknown Error";
}
