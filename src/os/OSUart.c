/*---------------------------------------------------------------------------*
  OSUart.c - UART Serial Port I/O
  
  On GC/Wii: Serial debugging via EXI UART
  On PC: Stub - use standard I/O instead
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>

/*---------------------------------------------------------------------------*
  Name:         InitializeUART

  Description:  Initialize UART for serial communication.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void InitializeUART(void) {
    /* No UART on PC - use stdout/stderr instead */
}

/*---------------------------------------------------------------------------*
  Name:         WriteUARTN

  Description:  Write N bytes to UART.

  Arguments:    buf   Buffer to write
                len   Number of bytes

  Returns:      Bytes written
 *---------------------------------------------------------------------------*/
u32 WriteUARTN(const void* buf, u32 len) {
    (void)buf;
    (void)len;
    
    /* On PC, output goes to OSReport/printf instead */
    return len;
}

/*---------------------------------------------------------------------------*
  Name:         ReadUARTN

  Description:  Read N bytes from UART.

  Arguments:    buf   Buffer to read into
                len   Number of bytes

  Returns:      Bytes read
 *---------------------------------------------------------------------------*/
u32 ReadUARTN(void* buf, u32 len) {
    (void)buf;
    (void)len;
    
    /* No UART input on PC */
    return 0;
}

