/*---------------------------------------------------------------------------*
  DVDLow.c - Low-Level DVD Commands
  
  Low-level DVD interface functions for direct hardware control.
  
  On GC/Wii: Sends commands directly to DI (Drive Interface) hardware
  On PC: Stubs - no DVD hardware, operations handled by high-level DVD.c
 *---------------------------------------------------------------------------*/

#include <dolphin/dvd.h>
#include <dolphin/os.h>

/*---------------------------------------------------------------------------*
  Name:         DVDLowInit

  Description:  Initialize low-level DVD system.
                
                On GC/Wii: Sets up DI registers, enables interrupts
                On PC: No-op stub

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void DVDLowInit(void) {
    /* No low-level DVD hardware on PC.
     * All initialization handled by DVDInit().
     */
}

/*---------------------------------------------------------------------------*
  Name:         DVDLowRead

  Description:  Low-level read command to DVD hardware.
                
                On GC/Wii: Sends read command to DI, starts DMA
                On PC: Not used - DVDRead() handles file I/O directly

  Arguments:    addr      Destination buffer
                length    Bytes to read
                offset    Disc offset
                callback  Callback when complete

  Returns:      TRUE if command issued
 *---------------------------------------------------------------------------*/
BOOL DVDLowRead(void* addr, u32 length, u32 offset, void (*callback)(u32)) {
    (void)addr;
    (void)length;
    (void)offset;
    (void)callback;
    
    /* Not implemented on PC.
     * Use DVDRead() or DVDReadAsync() instead.
     */
    return FALSE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDLowSeek

  Description:  Low-level seek command to DVD hardware.
                
                On GC/Wii: Commands disc head to seek position
                On PC: Not used - DVDSeek() handles directly

  Arguments:    offset    Disc offset to seek to
                callback  Callback when complete

  Returns:      TRUE if command issued
 *---------------------------------------------------------------------------*/
BOOL DVDLowSeek(u32 offset, void (*callback)(u32)) {
    (void)offset;
    (void)callback;
    
    /* Not implemented on PC.
     * Use DVDSeek() or DVDSeekAsync() instead.
     */
    return FALSE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDLowWaitCoverClose

  Description:  Wait for disc cover to be closed.
                
                On GC/Wii: Waits for cover sensor interrupt
                On PC: Returns immediately (no cover)

  Arguments:    callback  Callback when cover closed

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL DVDLowWaitCoverClose(void (*callback)(u32)) {
    /* No disc cover on PC.
     * Call callback immediately to signal "cover closed".
     */
    if (callback) {
        callback(0);
    }
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDLowReadDiskID

  Description:  Low-level read disc ID sector.
                
                On GC/Wii: Reads disc ID from sector 0
                On PC: Returns fake disc ID

  Arguments:    diskID    Buffer for disc ID
                callback  Callback when complete

  Returns:      TRUE if command issued
 *---------------------------------------------------------------------------*/
BOOL DVDLowReadDiskID(DVDDiskID* diskID, void (*callback)(u32)) {
    (void)diskID;
    
    /* Handled by DVDReadDiskID() instead.
     * This low-level version not needed on PC.
     */
    if (callback) {
        callback(0);
    }
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDLowStopMotor

  Description:  Stop disc motor.
                
                On GC/Wii: Sends motor stop command to drive
                On PC: No-op (no motor)

  Arguments:    callback  Callback when complete

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL DVDLowStopMotor(void (*callback)(u32)) {
    /* No disc motor on PC. */
    if (callback) {
        callback(0);
    }
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDLowRequestError

  Description:  Request error information from drive.
                
                On GC/Wii: Queries drive for last error
                On PC: Returns no error

  Arguments:    callback  Callback when complete

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL DVDLowRequestError(void (*callback)(u32)) {
    /* No drive errors on PC. */
    if (callback) {
        callback(0);  // No error
    }
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDLowReset

  Description:  Reset DVD drive.
                
                On GC/Wii: Sends reset command to DI
                On PC: No-op

  Arguments:    callback  Callback when complete

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL DVDLowReset(void (*callback)(u32)) {
    if (callback) {
        callback(0);
    }
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDLowBreak

  Description:  Stop current operation immediately.
                
                On GC/Wii: Sends break command to DI
                On PC: No-op

  Arguments:    None

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL DVDLowBreak(void) {
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDLowClearCallback

  Description:  Clear registered callback.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void DVDLowClearCallback(void) {
    /* Callbacks managed per-operation on PC, not globally. */
}

/*---------------------------------------------------------------------------*
  Name:         __DVDLowTestAlarm

  Description:  Test if alarm belongs to DVD low-level system.

  Arguments:    alarm  Alarm to test

  Returns:      FALSE always on PC (no DVD hardware alarms)
 *---------------------------------------------------------------------------*/
BOOL __DVDLowTestAlarm(const OSAlarm* alarm) {
    (void)alarm;
    return FALSE;
}

// Additional s32 callback variants for compatibility
BOOL DVDLowRead(void* addr, u32 length, u32 offset, void (*callback)(s32)) {
    (void)addr; (void)length; (void)offset;
    if (callback) callback(0);
    return TRUE;
}

BOOL DVDLowSeek(u32 offset, void (*callback)(s32)) {
    (void)offset;
    if (callback) callback(0);
    return TRUE;
}

BOOL DVDLowWaitCoverClose(void (*callback)(s32)) {
    if (callback) callback(0);
    return TRUE;
}

BOOL DVDLowStopMotor(void (*callback)(s32)) {
    if (callback) callback(0);
    return TRUE;
}

BOOL DVDLowReadDiskID(DVDDiskID* diskID, void (*callback)(s32)) {
    (void)diskID;
    if (callback) callback(0);
    return TRUE;
}

