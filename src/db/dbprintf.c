/*---------------------------------------------------------------------------*
  dbprintf.c - Debug Printf for libPorpoise
  
  PC-compatible version of DBPrintf.
  On PC, outputs to console using OSReport() instead of serial port.
 *---------------------------------------------------------------------------*/

#include <dolphin/types.h>
#include <dolphin/os.h>
#include <dolphin/db.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*
  Name:         DBPrintf
  
  Description:  Debug printf function.
                On hardware: Outputs to serial debugger port
                On PC: Outputs to console using OSReport()
  
  Arguments:    format - printf-style format string
                ...    - Variable arguments
  
  Returns:      None.
 *---------------------------------------------------------------------------*/
void DBPrintf(const char *format, ...) {
    va_list args;
    char buffer[1024];
    
    if (format == NULL) {
        return;
    }
    
    /* Format the string */
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    /* Ensure null termination */
    buffer[sizeof(buffer) - 1] = '\0';
    
    /* Output to console using OSReport */
    /* Note: OSReport already adds timestamp, so we just output the message */
    OSReport("%s", buffer);
}

