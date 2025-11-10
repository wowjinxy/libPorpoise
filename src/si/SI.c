/*---------------------------------------------------------------------------*
  SI.c - Serial Interface (Controller Port Hardware)
  
  The Serial Interface handles communication with controller ports.
  
  On GC/Wii: Hardware SI handles controller polling automatically
  On PC: PAD module handles controllers directly via SDL2, SI is stubbed
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <dolphin/types.h>

/*---------------------------------------------------------------------------*
  Name:         SIInit

  Description:  Initialize Serial Interface hardware.
                
                On GC/Wii: Initializes SI registers and interrupts
                On PC: No-op (PAD module uses SDL2 instead)

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void SIInit(void) {
    /* Serial Interface not used on PC.
     * Controller input handled by PAD module via SDL2.
     */
}

/*---------------------------------------------------------------------------*
  Name:         SIGetStatus

  Description:  Get SI channel status register.

  Arguments:    chan  Channel number (0-3)

  Returns:      Status register value (0 = OK on PC)
 *---------------------------------------------------------------------------*/
u32 SIGetStatus(s32 chan) {
    (void)chan;
    return 0;  // No errors
}

/*---------------------------------------------------------------------------*
  Name:         SIGetResponse

  Description:  Get response data from SI channel.

  Arguments:    chan  Channel number
                data  Pointer to receive response (2 u32s)

  Returns:      TRUE if response available
 *---------------------------------------------------------------------------*/
BOOL SIGetResponse(s32 chan, u32* data) {
    (void)chan;
    if (data) {
        data[0] = 0;
        data[1] = 0;
    }
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         SISetCommand

  Description:  Set SI command for channel.

  Arguments:    chan  Channel number
                cmd   Command value

  Returns:      None
 *---------------------------------------------------------------------------*/
void SISetCommand(s32 chan, u32 cmd) {
    (void)chan;
    (void)cmd;
}

/*---------------------------------------------------------------------------*
  Name:         SITransfer

  Description:  Transfer data to/from SI device.

  Arguments:    chan      Channel number
                output    Output buffer
                outBytes  Output size
                input     Input buffer
                inBytes   Input size
                callback  Callback when complete
                delay     Delay (unused)

  Returns:      TRUE if transfer started
 *---------------------------------------------------------------------------*/
BOOL SITransfer(s32 chan, void* output, u32 outBytes, void* input, u32 inBytes,
                void (*callback)(s32, u32, void*), u32 delay) {
    (void)chan;
    (void)output;
    (void)outBytes;
    (void)input;
    (void)inBytes;
    (void)delay;
    
    /* Not implemented on PC.
     * PAD module handles controller communication via SDL2.
     */
    if (callback) {
        callback(chan, 0, NULL);  // Signal complete
    }
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         SIGetType

  Description:  Get device type on SI channel.

  Arguments:    chan  Channel number

  Returns:      Device type (0x09000000 = standard controller)
 *---------------------------------------------------------------------------*/
u32 SIGetType(s32 chan) {
    (void)chan;
    return 0x09000000;  // Standard controller type
}

/*---------------------------------------------------------------------------*
  Name:         SIGetTypeAsync

  Description:  Get device type asynchronously.

  Arguments:    chan      Channel number
                callback  Callback with type result

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL SIGetTypeAsync(s32 chan, void (*callback)(s32, u32)) {
    u32 type = SIGetType(chan);
    if (callback) {
        callback(chan, type);
    }
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         SIEnablePolling

  Description:  Enable automatic polling for channels.

  Arguments:    poll  Channel bits to enable

  Returns:      None
 *---------------------------------------------------------------------------*/
void SIEnablePolling(u32 poll) {
    (void)poll;
}

/*---------------------------------------------------------------------------*
  Name:         SIDisablePolling

  Description:  Disable automatic polling for channels.

  Arguments:    poll  Channel bits to disable

  Returns:      None
 *---------------------------------------------------------------------------*/
void SIDisablePolling(u32 poll) {
    (void)poll;
}

/*---------------------------------------------------------------------------*
  Name:         SIBusy

  Description:  Check if SI is busy with transfer.

  Arguments:    None

  Returns:      FALSE always on PC (no hardware)
 *---------------------------------------------------------------------------*/
BOOL SIBusy(void) {
    return FALSE;
}

/*---------------------------------------------------------------------------*
  Name:         SIIsChanBusy

  Description:  Check if specific channel is busy.

  Arguments:    chan  Channel number

  Returns:      FALSE always on PC
 *---------------------------------------------------------------------------*/
BOOL SIIsChanBusy(s32 chan) {
    (void)chan;
    return FALSE;
}

/*---------------------------------------------------------------------------*
  Name:         SISetSamplingRate

  Description:  Set controller sampling rate.

  Arguments:    msec  Sampling rate in milliseconds

  Returns:      None
 *---------------------------------------------------------------------------*/
void SISetSamplingRate(u32 msec) {
    (void)msec;
}

/*---------------------------------------------------------------------------*
  Name:         SIRefreshSamplingRate

  Description:  Refresh sampling rate based on video mode.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void SIRefreshSamplingRate(void) {
    /* No-op on PC */
}

/*---------------------------------------------------------------------------*
  Name:         SIRegisterPollingHandler

  Description:  Register handler called during SI polling.

  Arguments:    handler  Handler function

  Returns:      None
 *---------------------------------------------------------------------------*/
void SIRegisterPollingHandler(void (*handler)(void*, void*)) {
    (void)handler;
}

/*---------------------------------------------------------------------------*
  Name:         SIUnregisterPollingHandler

  Description:  Unregister polling handler.

  Arguments:    handler  Handler function

  Returns:      None
 *---------------------------------------------------------------------------*/
void SIUnregisterPollingHandler(void (*handler)(void*, void*)) {
    (void)handler;
}

/*---------------------------------------------------------------------------*
  Name:         SITransferCommands

  Description:  Execute queued SI commands.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void SITransferCommands(void) {
    /* No-op on PC */
}

/*---------------------------------------------------------------------------*
  Name:         OSSetWirelessID

  Description:  Set wireless controller ID.

  Arguments:    chan  Channel number
                id    Wireless ID

  Returns:      None
 *---------------------------------------------------------------------------*/
void OSSetWirelessID(s32 chan, u16 id) {
    (void)chan;
    (void)id;
}

