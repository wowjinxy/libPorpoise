/*---------------------------------------------------------------------------*
  db.h - Dolphin Debugger Interface for libPorpoise

  PC-compatible version of the Dolphin Debugger interface.
  Provides debugger communication functions for development.

  On PC: Most functions output to console instead of serial debugger.
 *---------------------------------------------------------------------------*/

#ifndef DOLPHIN_DB_H
#define DOLPHIN_DB_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

    /*---------------------------------------------------------------------------*
        DBInterface Structure

        Used to communicate with the Dolphin debugger kernel.
        On PC, this is mostly unused but kept for API compatibility.
     *---------------------------------------------------------------------------*/
    typedef struct DBInterface {
        u32     bPresent;       // non zero if debug kernel is loaded
        u32     exceptionMask;  // bitmask of exceptions

        // ptr to the destination so it does not have to be in low
        // memory, nor do we have to perform 2 loads
        void    (*ExceptionDestination)(void);

        // Return address from DBExceptionDestination that debugger should
        // jump to.
        void* exceptionReturn;
    } DBInterface;

    // Global debugger interface pointer
    extern DBInterface* __DBInterface;

    /*---------------------------------------------------------------------------*
        Public Functions
     *---------------------------------------------------------------------------*/

     /* Initialize the debugger interface */
    void DBInit(void);

    /* Check if debugger is present */
    BOOL DBIsDebuggerPresent(void);

    /* Debug printf - outputs to console on PC */
    void DBPrintf(const char* format, ...);

    /* Verbose flag - controls debug output verbosity */
    extern BOOL DBVerbose;

    /*---------------------------------------------------------------------------*
        Internal Functions (for exception handling)
     *---------------------------------------------------------------------------*/

     /* Check if exception is marked for debugger */
    BOOL __DBIsExceptionMarked(int exception);

    /* Exception destination (called from exception handlers) */
    void __DBExceptionDestination(void);

#ifdef _DEBUG
    /* Testing functions (debug builds only) */
    void __DBSetPresent(u32 value);
    void __DBMarkException(int exception, BOOL value);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_DB_H */

