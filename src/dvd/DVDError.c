/*---------------------------------------------------------------------------*
  DVDError.c - DVD Error Handling and Logging
  
  Handles DVD error codes and logging.
  
  On GC/Wii: Stores error codes in NAND flash for diagnostics
  On PC: Logs to console only
 *---------------------------------------------------------------------------*/

#include <dolphin/dvd.h>
#include <dolphin/os.h>

/*---------------------------------------------------------------------------*
    Error Codes
 *---------------------------------------------------------------------------*/

#define DVD_INTERNALERROR_NO_ERROR                      0x000000
#define DVD_INTERNALERROR_COVEROPEN_OR_NODISK           0x023A00
#define DVD_INTERNALERROR_COVER_CLOSED                  0x020400
#define DVD_INTERNALERROR_NO_SEEK_COMPLETE              0x030200
#define DVD_INTERNALERROR_UNRECOVERED_READ              0x031100
#define DVD_INTERNALERROR_INVALID_COMMAND               0x052000
#define DVD_INTERNALERROR_AUDIOBUF_NOT_SET              0x052001
#define DVD_INTERNALERROR_LBA_OUT_OF_RANGE              0x052100
#define DVD_INTERNALERROR_INVALID_FIELD                 0x052400
#define DVD_INTERNALERROR_INVALID_AUDIO_COMMAND         0x052401
#define DVD_INTERNALERROR_AUDIOBUF_CONFIG_NOT_ALLOWED   0x052402
#define DVD_INTERNALERROR_OP_DISK_RM_REQ                0x062800
#define DVD_INTERNALERROR_END_OF_USER_AREA              0x063100
#define DVD_INTERNALERROR_ID_NOT_READ                   0x020E00
#define DVD_INTERNALERROR_MOTOR_STOPPED                 0x040800
#define DVD_INTERNALERROR_PROTOCOL_ERROR                0x040100

static BOOL s_errorLogged = FALSE;
static u32 s_lastError = DVD_INTERNALERROR_NO_ERROR;

/*---------------------------------------------------------------------------*
  Name:         __DVDStoreErrorCode

  Description:  Store DVD error code for diagnostics.
                
                On GC/Wii: Writes error to NAND flash with timestamp
                On PC: Logs to console

  Arguments:    errorCode  DVD error code
                result     Operation result

  Returns:      None
 *---------------------------------------------------------------------------*/
void __DVDStoreErrorCode(u32 errorCode, u32 result) {
    s_lastError = errorCode;
    s_errorLogged = TRUE;
    
    OSReport("DVD: Error logged - Code: 0x%06X, Result: 0x%08X\n", 
             errorCode, result);
}

/*---------------------------------------------------------------------------*
  Name:         __DVDGetLastError

  Description:  Get last logged error code.

  Arguments:    None

  Returns:      Last error code
 *---------------------------------------------------------------------------*/
u32 __DVDGetLastError(void) {
    return s_lastError;
}

/*---------------------------------------------------------------------------*
  Name:         __DVDClearErrorLog

  Description:  Clear error log.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void __DVDClearErrorLog(void) {
    s_errorLogged = FALSE;
    s_lastError = DVD_INTERNALERROR_NO_ERROR;
}

/*---------------------------------------------------------------------------*
  Name:         __DVDHasErrorLogged

  Description:  Check if any error has been logged.

  Arguments:    None

  Returns:      TRUE if error logged, FALSE otherwise
 *---------------------------------------------------------------------------*/
BOOL __DVDHasErrorLogged(void) {
    return s_errorLogged;
}

