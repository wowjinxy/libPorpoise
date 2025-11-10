/*---------------------------------------------------------------------------*
  DVDFatal.c - DVD Fatal Error Messages
  
  Displays fatal error messages when DVD operations fail critically.
  
  On GC/Wii: Shows on-screen error message, stops game
  On PC: Logs error to console
 *---------------------------------------------------------------------------*/

#include <dolphin/dvd.h>
#include <dolphin/os.h>

/*---------------------------------------------------------------------------*
  Name:         __DVDShowFatalMessage

  Description:  Display fatal DVD error message.
                
                On GC/Wii: Renders error screen, halts system
                On PC: Logs error and calls OSPanic

  Arguments:    None

  Returns:      Does not return
 *---------------------------------------------------------------------------*/
void __DVDShowFatalMessage(void) {
    OSReport("==================================================\n");
    OSReport("DVD FATAL ERROR\n");
    OSReport("==================================================\n");
    OSReport("A fatal DVD error has occurred.\n");
    OSReport("Please check:\n");
    OSReport("  - files/ directory exists\n");
    OSReport("  - File paths are correct\n");
    OSReport("  - Files are not corrupted\n");
    OSReport("==================================================\n");
    
    OSPanic(__FILE__, __LINE__, "DVD Fatal Error");
}

/*---------------------------------------------------------------------------*
  Name:         __DVDPrintFatalMessage

  Description:  Print fatal error message to console only.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void __DVDPrintFatalMessage(void) {
    OSReport("[DVD FATAL] Critical error occurred\n");
}

