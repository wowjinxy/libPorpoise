/*---------------------------------------------------------------------------*
  EXI.c - External Interface
  
  ARCHITECTURAL DIFFERENCES: GC/Wii vs PC
  ========================================
  
  On GC/Wii (EXI Hardware):
  --------------------------
  - 3 EXI channels (0, 1, 2)
  - Channel 0: Memory Card Slot A + Serial Port 1
  - Channel 1: Memory Card Slot B + Serial Port 2  
  - Channel 2: Broadband/Modem Adapter
  - Each channel can have up to 3 devices
  - Hardware-controlled select/lock/transfer
  - DMA transfers for large data
  - Used by: CARD (memory cards), SI (controllers), network adapters
  
  On PC (No EXI Hardware):
  ------------------------
  - All functions are stubs
  - CARD module will simulate memory cards with files
  - No actual external devices
  - Functions return success immediately
  
  WHY STUB:
  - No physical EXI hardware on PC
  - Memory card I/O handled by CARD module using files
  - Serial ports not used (controllers via SDL2/PAD)
  - Network via standard PC APIs
  
  WHAT WE PRESERVE:
  - Same API for compatibility
  - Functions return success
  - Games can call EXI functions without crashing
 *---------------------------------------------------------------------------*/

#include <dolphin/exi.h>
#include <dolphin/os.h>

/*---------------------------------------------------------------------------*
  Name:         EXIAttach

  Description:  Attach to EXI channel.

  Arguments:    chan              Channel number
                detachCallback    Callback when device detached

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL EXIAttach(s32 chan, EXICallback detachCallback) {
    (void)chan;
    (void)detachCallback;
    
    /* No EXI hardware on PC. */
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         EXIDetach

  Description:  Detach from EXI channel.

  Arguments:    chan  Channel number

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL EXIDetach(s32 chan) {
    (void)chan;
    
    /* No EXI hardware on PC. */
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         EXILock

  Description:  Lock EXI channel for exclusive access.

  Arguments:    chan              Channel number
                dev               Device number
                unlockCallback    Callback when unlocked

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL EXILock(s32 chan, u32 dev, EXICallback unlockCallback) {
    (void)chan;
    (void)dev;
    (void)unlockCallback;
    
    /* No locking needed on PC. */
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         EXIUnlock

  Description:  Unlock EXI channel.

  Arguments:    chan  Channel number

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL EXIUnlock(s32 chan) {
    (void)chan;
    
    /* No locking needed on PC. */
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         EXISelect

  Description:  Select EXI device for communication.

  Arguments:    chan  Channel number
                dev   Device number
                freq  Communication frequency

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL EXISelect(s32 chan, u32 dev, u32 freq) {
    (void)chan;
    (void)dev;
    (void)freq;
    
    /* No device selection on PC. */
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         EXIDeselect

  Description:  Deselect EXI device.

  Arguments:    chan  Channel number

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL EXIDeselect(s32 chan) {
    (void)chan;
    
    /* No device selection on PC. */
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         EXIImm

  Description:  Immediate data transfer (for small amounts).

  Arguments:    chan      Channel number
                data      Data buffer
                len       Length in bytes
                mode      Transfer mode (read/write)
                callback  Completion callback

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL EXIImm(s32 chan, void* data, u32 len, u32 mode, EXICallback callback) {
    (void)chan;
    (void)data;
    (void)len;
    (void)mode;
    
    /* No data transfer on PC. */
    
    if (callback) {
        callback(chan, NULL);
    }
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         EXIImmEx

  Description:  Immediate data transfer (extended).

  Arguments:    chan  Channel number
                data  Data buffer
                len   Length in bytes
                mode  Transfer mode

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL EXIImmEx(s32 chan, void* data, u32 len, u32 mode) {
    (void)chan;
    (void)data;
    (void)len;
    (void)mode;
    
    /* No data transfer on PC. */
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         EXIDma

  Description:  DMA data transfer (for large amounts).

  Arguments:    chan      Channel number
                buf       Data buffer
                len       Length in bytes
                mode      Transfer mode
                callback  Completion callback

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL EXIDma(s32 chan, void* buf, u32 len, u32 mode, EXICallback callback) {
    (void)chan;
    (void)buf;
    (void)len;
    (void)mode;
    
    /* No DMA on PC. */
    
    if (callback) {
        callback(chan, NULL);
    }
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         EXISync

  Description:  Wait for transfer to complete.

  Arguments:    chan  Channel number

  Returns:      TRUE always (transfers instant on PC)
 *---------------------------------------------------------------------------*/
BOOL EXISync(s32 chan) {
    (void)chan;
    
    /* Transfers are instant on PC. */
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         EXIProbe

  Description:  Probe for device presence.

  Arguments:    chan  Channel number

  Returns:      FALSE (no devices on PC)
 *---------------------------------------------------------------------------*/
BOOL EXIProbe(s32 chan) {
    (void)chan;
    
    /* No physical devices on PC.
     * Return FALSE so games know not to use EXI hardware.
     */
    return FALSE;
}

/*---------------------------------------------------------------------------*
  Name:         EXIProbeEx

  Description:  Extended device probe.

  Arguments:    chan  Channel number

  Returns:      0 (no devices)
 *---------------------------------------------------------------------------*/
s32 EXIProbeEx(s32 chan) {
    (void)chan;
    
    /* No devices on PC. */
    return 0;
}

/*---------------------------------------------------------------------------*
  Name:         EXIGetID

  Description:  Get device ID.

  Arguments:    chan  Channel number
                dev   Device number
                id    Pointer to receive ID

  Returns:      FALSE (no devices)
 *---------------------------------------------------------------------------*/
BOOL EXIGetID(s32 chan, u32 dev, u32* id) {
    (void)chan;
    (void)dev;
    
    if (id) {
        *id = 0;  // No device
    }
    
    return FALSE;
}

/*---------------------------------------------------------------------------*
  Name:         EXIGetState

  Description:  Get transfer state.

  Arguments:    chan  Channel number

  Returns:      0 (idle)
 *---------------------------------------------------------------------------*/
u32 EXIGetState(s32 chan) {
    (void)chan;
    
    /* Always idle on PC. */
    return 0;
}

/*---------------------------------------------------------------------------*
  Name:         EXIInit

  Description:  Initialize EXI subsystem.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void EXIInit(void) {
    /* No initialization needed on PC. */
    OSReport("EXI: External Interface (stub mode)\n");
}

/*---------------------------------------------------------------------------*
  Name:         EXIClearInterrupts

  Description:  Clear EXI interrupt flags.

  Arguments:    chan  Channel number
                exi   Clear EXI interrupt
                tc    Clear transfer complete interrupt
                ext   Clear external interrupt

  Returns:      Previous interrupt state
 *---------------------------------------------------------------------------*/
u32 EXIClearInterrupts(s32 chan, BOOL exi, BOOL tc, BOOL ext) {
    (void)chan;
    (void)exi;
    (void)tc;
    (void)ext;
    
    /* No interrupts on PC. */
    return 0;
}

/*---------------------------------------------------------------------------*
  Name:         EXIProbeReset

  Description:  Reset probe state.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void EXIProbeReset(void) {
    /* No probe state on PC. */
}

/*---------------------------------------------------------------------------*
  Name:         EXIGetType

  Description:  Get device type.

  Arguments:    chan  Channel number
                dev   Device number
                type  Pointer to receive type

  Returns:      0 (no device)
 *---------------------------------------------------------------------------*/
s32 EXIGetType(s32 chan, u32 dev, u32* type) {
    (void)chan;
    (void)dev;
    
    if (type) {
        *type = 0;  // No device
    }
    
    return 0;
}

/*---------------------------------------------------------------------------*
  Name:         EXISelectSD

  Description:  Select device (SD card specific).

  Arguments:    chan  Channel number
                dev   Device number
                freq  Communication frequency

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL EXISelectSD(s32 chan, u32 dev, u32 freq) {
    return EXISelect(chan, dev, freq);
}

/*---------------------------------------------------------------------------*
  Name:         EXIGetIDEx

  Description:  Get device ID (extended).

  Arguments:    chan  Channel number
                dev   Device number
                id    Pointer to receive ID

  Returns:      0 (no device)
 *---------------------------------------------------------------------------*/
s32 EXIGetIDEx(s32 chan, u32 dev, u32* id) {
    (void)chan;
    (void)dev;
    
    if (id) {
        *id = 0;
    }
    
    return 0;
}

/*---------------------------------------------------------------------------*
  Name:         EXIGetConsoleType

  Description:  Get console type.

  Arguments:    None

  Returns:      0 (generic)
 *---------------------------------------------------------------------------*/
u32 EXIGetConsoleType(void) {
    return 0;  // Generic
}

/*---------------------------------------------------------------------------*
  Name:         EXIWait

  Description:  Wait for EXI operation.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void EXIWait(void) {
    /* No wait needed on PC. */
}

/*---------------------------------------------------------------------------*
  Name:         EXISetExiCallback

  Description:  Set EXI interrupt callback.

  Arguments:    chan         Channel number
                exiCallback  Callback function

  Returns:      Previous callback
 *---------------------------------------------------------------------------*/
EXICallback EXISetExiCallback(s32 chan, EXICallback exiCallback) {
    (void)chan;
    (void)exiCallback;
    
    /* No interrupts on PC. */
    return NULL;
}


