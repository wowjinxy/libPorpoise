/*---------------------------------------------------------------------------*
  SI.c - Serial Interface (Controller Port Hardware)
  
  ARCHITECTURAL DIFFERENCES: GC/Wii vs PC
  ========================================
  
  On GC/Wii (Serial Interface Hardware):
  ---------------------------------------
  - Hardware SI handles controller polling automatically
  - SI registers (COMCSR, SR, OUTBUF, INBUF) control communication
  - Hardware DMA transfers controller data to memory
  - Supports 4 controller ports (SI channels 0-3)
  - Controllers send 8-byte packets with button/analog state
  - Transfer timing: ~7600ns + 1050ns + 2000ns + 1050ns + 4200ns * 8 * bytes
  - Generates SI interrupt when transfer completes (TCINT bit)
  - PI_INTSR tracks interrupt status across modules
  
  On PC (PAD Module + SI State Tracking):
  ----------------------------------------
  - PAD module uses SDL2 for controller input (direct API)
  - SI module tracks register state for API compatibility
  - No hardware DMA - PAD reads directly from SDL2
  - SI state tracking allows games to check SI status
  - Transfer timing simulated with OSAlarm (like PPCArthur)
  - Interrupt status tracked in PI_INTSR
  
  WHAT WE PRESERVE:
  - Same API surface (SIInit, SITransfer, SIGetStatus, etc.)
  - Register state tracking (like PPCArthur __SIEmuRegs)
  - Transfer timing simulation (OSAlarm-based)
  - Interrupt status tracking (PI_INTSR)
  - State transition detection (when TSTART is set)
  
  WHAT'S DIFFERENT:
  - No hardware SI polling - PAD module handles controllers
  - Transfers are instant (no actual hardware timing)
  - Interrupts are simulated (no hardware interrupts)
  - Controller data comes from PAD module, not SI hardware
  
  INSPIRED BY PPCARTHUR:
  - Register state tracking (__SIEmuRegs pattern)
  - OSAlarm-based transfer timing
  - State transition detection
  - Interrupt status tracking
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <dolphin/si.h>
#include <dolphin/os/OSAlarm.h>
#include <string.h>
#include <stdlib.h>

/*---------------------------------------------------------------------------*
    SI Register Definitions (inspired by PPCArthur)
 *---------------------------------------------------------------------------*/

// SI register indices (simplified - full set would match hardware)
#define SI_REG_COMCSR     0  // Command/Status register
#define SI_REG_SR         1  // Status register
#define SI_REG_POLL       2  // Polling control
#define SI_REG_EXILK      3  // EXI lock
#define SI_REG_OUTBUF(n)  (4 + (n) * 3)  // Output buffer for channel n
#define SI_REG_INBUFH(n)  (5 + (n) * 3)  // Input buffer high for channel n
#define SI_REG_INBUFL(n)  (6 + (n) * 3)  // Input buffer low for channel n
#define SI_REG_MAX        16

// COMCSR bits
#define SI_COMCSR_TSTART      (1 << 0)   // Transfer start
#define SI_COMCSR_TCINT       (1 << 1)   // Transfer complete interrupt
#define SI_COMCSR_TCINTMSK    (1 << 2)   // Transfer complete interrupt mask
#define SI_COMCSR_CHANNEL_MASK 0x0C      // Channel select (bits 2-3)
#define SI_COMCSR_OUTLNGTH_MASK 0xF0     // Output length (bits 4-7)
#define SI_COMCSR_INLNGTH_MASK  0xF00    // Input length (bits 8-11)

// SR bits
#define SI_SR_WR              (1 << 0)   // Write request
#define SI_SR_NOREP0          (1 << 8)   // No response channel 0
#define SI_SR_NOREP1          (1 << 16)  // No response channel 1
#define SI_SR_NOREP2          (1 << 24)  // No response channel 2
#define SI_SR_NOREP3          (1 << 31)  // No response channel 3

// PI interrupt status (shared across modules)
static u32 s_piIntsr = 0;
#define PI_INTSR_REG_SIINT_MASK (1 << 0)  // SI interrupt mask

/*---------------------------------------------------------------------------*
    SI Register State (like PPCArthur __SIEmuRegs)
 *---------------------------------------------------------------------------*/

// SI emulated register state
static u32 s_siRegs[SI_REG_MAX] = {0};

// Output buffers (stored when WR bit is set)
static u32 s_siOutbufs[SI_MAX_CHAN] = {0};

// Transfer state
typedef struct {
    s32 chan;                              // Channel number
    void* output;                          // Output buffer
    u32 outBytes;                          // Output size
    void* input;                           // Input buffer
    u32 inBytes;                           // Input size
    void (*callback)(s32, u32, void*);     // Completion callback
    OSAlarm alarm;                         // Timing alarm
    BOOL active;                           // TRUE if transfer active
} SITransferState;

static SITransferState s_transferState = {0};

// Initialization flag
static BOOL s_initialized = FALSE;

/*---------------------------------------------------------------------------*
  Name:         TransferAlarmHandler

  Description:  Alarm handler for SI transfer completion.
                Inspired by PPCArthur's AlarmHandler().
                Called when transfer timing completes.

  Arguments:    alarm   Transfer timing alarm
                context Current context

  Returns:      None
 *---------------------------------------------------------------------------*/
static void TransferAlarmHandler(OSAlarm* alarm, OSContext* context) {
    (void)alarm;
    (void)context;
    
    if (!s_transferState.active) {
        return;
    }
    
    // Clear TSTART bit (transfer complete)
    s_siRegs[SI_REG_COMCSR] &= ~SI_COMCSR_TSTART;
    
    // Set TCINT bit (transfer complete interrupt)
    s_siRegs[SI_REG_COMCSR] |= SI_COMCSR_TCINT;
    
    // Set interrupt status if enabled
    if ((s_siRegs[SI_REG_COMCSR] & SI_COMCSR_TCINT) &&
        (s_siRegs[SI_REG_COMCSR] & SI_COMCSR_TCINTMSK)) {
        s_piIntsr |= PI_INTSR_REG_SIINT_MASK;
    } else {
        s_piIntsr &= ~PI_INTSR_REG_SIINT_MASK;
    }
    
    // Call completion callback
    if (s_transferState.callback) {
        s_transferState.callback(s_transferState.chan, 0, NULL);
    }
    
    // Clear transfer state
    s_transferState.active = FALSE;
}

/*---------------------------------------------------------------------------*
  Name:         CalculateTransferTime

  Description:  Calculate transfer timing based on byte count.
                Inspired by PPCArthur's timing calculation.

  Arguments:    outBytes  Output byte count
                inBytes   Input byte count

  Returns:      Transfer time in nanoseconds
 *---------------------------------------------------------------------------*/
static OSTime CalculateTransferTime(u32 outBytes, u32 inBytes) {
    // PPCArthur timing: 7600 + 1050 + 2000 + 1050 + 4200 * 8 * bytes
    // Base overhead: 7600ns + 1050ns + 2000ns + 1050ns = 11700ns
    // Per byte: 4200ns * 8 = 33600ns per byte
    u64 baseTime = 11700;  // Base overhead (nanoseconds)
    u64 byteTime = 33600;  // Per byte (nanoseconds)
    u64 totalBytes = (outBytes + inBytes);
    u64 totalTime = baseTime + (byteTime * totalBytes);
    
    return OSNanosecondsToTicks(totalTime);
}

/*---------------------------------------------------------------------------*
  Name:         SIInit

  Description:  Initialize Serial Interface hardware.
                
                On GC/Wii: Initializes SI registers and interrupts
                On PC: Initializes SI state tracking (PAD uses SDL2)

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void SIInit(void) {
    if (s_initialized) {
        return;
    }
    
    // Initialize register state (like PPCArthur)
    memset(s_siRegs, 0, sizeof(s_siRegs));
    memset(s_siOutbufs, 0, sizeof(s_siOutbufs));
    memset(&s_transferState, 0, sizeof(s_transferState));
    s_piIntsr = 0;
    
    // Initialize transfer alarm
    OSCreateAlarm(&s_transferState.alarm);
    
    s_initialized = TRUE;
    OSReport("SI: Serial Interface initialized (state tracking)\n");
    OSReport("SI: Controller input handled by PAD module via SDL2\n");
}

/*---------------------------------------------------------------------------*
  Name:         SIGetStatus

  Description:  Get SI channel status register.

  Arguments:    chan  Channel number (0-3)

  Returns:      Status register value
 *---------------------------------------------------------------------------*/
u32 SIGetStatus(s32 chan) {
    if (chan < 0 || chan >= SI_MAX_CHAN) {
        return 0;
    }
    
    // Return status from SR register
    // Check channel-specific status bits
    u32 sr = s_siRegs[SI_REG_SR];
    u32 status = 0;
    
    switch (chan) {
        case 0:
            if (sr & SI_SR_NOREP0) {
                status |= 0x80000000;  // No response
            }
            break;
        case 1:
            if (sr & SI_SR_NOREP1) {
                status |= 0x80000000;  // No response
            }
            break;
        case 2:
            if (sr & SI_SR_NOREP2) {
                status |= 0x80000000;  // No response
            }
            break;
        case 3:
            if (sr & SI_SR_NOREP3) {
                status |= 0x80000000;  // No response
            }
            break;
    }
    
    return status;
}

/*---------------------------------------------------------------------------*
  Name:         SIGetResponse

  Description:  Get response data from SI channel.

  Arguments:    chan  Channel number
                data  Pointer to receive response (2 u32s)

  Returns:      TRUE if response available
 *---------------------------------------------------------------------------*/
BOOL SIGetResponse(s32 chan, u32* data) {
    if (chan < 0 || chan >= SI_MAX_CHAN || !data) {
        return FALSE;
    }
    
    // Get response from INBUF registers
    u32 inbufh = s_siRegs[SI_REG_INBUFH(chan)];
    u32 inbufl = s_siRegs[SI_REG_INBUFL(chan)];
    
    data[0] = inbufh;
    data[1] = inbufl;
    
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
    if (chan < 0 || chan >= SI_MAX_CHAN) {
        return;
    }
    
    // Store command in OUTBUF register
    s_siRegs[SI_REG_OUTBUF(chan)] = cmd;
    s_siOutbufs[chan] = cmd;
}

/*---------------------------------------------------------------------------*
  Name:         SITransfer

  Description:  Transfer data to/from SI device.
                Inspired by PPCArthur's transfer timing simulation.

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
    (void)delay;
    
    if (chan < 0 || chan >= SI_MAX_CHAN) {
        return FALSE;
    }
    
    // Cancel any existing transfer
    if (s_transferState.active) {
        OSCancelAlarm(&s_transferState.alarm);
        s_transferState.active = FALSE;
    }
    
    // Store transfer state
    s_transferState.chan = chan;
    s_transferState.output = output;
    s_transferState.outBytes = outBytes;
    s_transferState.input = input;
    s_transferState.inBytes = inBytes;
    s_transferState.callback = callback;
    s_transferState.active = TRUE;
    
    // Update COMCSR register
    s_siRegs[SI_REG_COMCSR] &= ~SI_COMCSR_TCINT;  // Clear interrupt
    s_siRegs[SI_REG_COMCSR] |= SI_COMCSR_TSTART;  // Set transfer start
    s_siRegs[SI_REG_COMCSR] &= ~SI_COMCSR_CHANNEL_MASK;
    s_siRegs[SI_REG_COMCSR] |= (chan << 2) & SI_COMCSR_CHANNEL_MASK;
    s_siRegs[SI_REG_COMCSR] &= ~SI_COMCSR_OUTLNGTH_MASK;
    s_siRegs[SI_REG_COMCSR] |= (outBytes << 4) & SI_COMCSR_OUTLNGTH_MASK;
    s_siRegs[SI_REG_COMCSR] &= ~SI_COMCSR_INLNGTH_MASK;
    s_siRegs[SI_REG_COMCSR] |= (inBytes << 8) & SI_COMCSR_INLNGTH_MASK;
    
    // Store output buffer
    if (output && outBytes > 0) {
        s_siOutbufs[chan] = *(u32*)output;  // Store first word
    }
    
    // Calculate transfer time (like PPCArthur)
    OSTime transferTime = CalculateTransferTime(outBytes, inBytes);
    
    // Set alarm for transfer completion (like PPCArthur)
    // On PC, transfers are instant, but we simulate timing for compatibility
    OSSetAlarm(&s_transferState.alarm, transferTime, TransferAlarmHandler);
    
    // Note: On original hardware, this would trigger hardware DMA
    // On PC, PAD module handles controller communication via SDL2
    // This is just state tracking for API compatibility
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         SIGetType

  Description:  Get device type on SI channel.

  Arguments:    chan  Channel number

  Returns:      Device type (0x09000000 = standard controller)
 *---------------------------------------------------------------------------*/
u32 SIGetType(s32 chan) {
    if (chan < 0 || chan >= SI_MAX_CHAN) {
        return 0;
    }
    
    // Standard controller type (like PPCArthur __SIDolphinController)
    return 0x09000100;  // Controller present, standard type
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
    // Update POLL register (enable bits)
    s_siRegs[SI_REG_POLL] |= poll;
}

/*---------------------------------------------------------------------------*
  Name:         SIDisablePolling

  Description:  Disable automatic polling for channels.

  Arguments:    poll  Channel bits to disable

  Returns:      None
 *---------------------------------------------------------------------------*/
void SIDisablePolling(u32 poll) {
    // Update POLL register (disable bits)
    s_siRegs[SI_REG_POLL] &= ~poll;
}

/*---------------------------------------------------------------------------*
  Name:         SIBusy

  Description:  Check if SI is busy with transfer.

  Arguments:    None

  Returns:      TRUE if busy, FALSE otherwise
 *---------------------------------------------------------------------------*/
BOOL SIBusy(void) {
    // Check if transfer is active
    if (s_transferState.active) {
        return TRUE;
    }
    
    // Check TSTART bit in COMCSR
    if (s_siRegs[SI_REG_COMCSR] & SI_COMCSR_TSTART) {
        return TRUE;
    }
    
    return FALSE;
}

/*---------------------------------------------------------------------------*
  Name:         SIIsChanBusy

  Description:  Check if specific channel is busy.

  Arguments:    chan  Channel number

  Returns:      TRUE if busy, FALSE otherwise
 *---------------------------------------------------------------------------*/
BOOL SIIsChanBusy(s32 chan) {
    if (chan < 0 || chan >= SI_MAX_CHAN) {
        return FALSE;
    }
    
    // Check if this channel is active
    if (s_transferState.active && s_transferState.chan == chan) {
        return TRUE;
    }
    
    // Check COMCSR channel select
    u32 channel = (s_siRegs[SI_REG_COMCSR] & SI_COMCSR_CHANNEL_MASK) >> 2;
    if (channel == chan && (s_siRegs[SI_REG_COMCSR] & SI_COMCSR_TSTART)) {
        return TRUE;
    }
    
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
    // Sampling rate controlled by PAD module on PC
}

/*---------------------------------------------------------------------------*
  Name:         SIRefreshSamplingRate

  Description:  Refresh sampling rate based on video mode.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void SIRefreshSamplingRate(void) {
    // No-op on PC (PAD module handles this)
}

/*---------------------------------------------------------------------------*
  Name:         SIRegisterPollingHandler

  Description:  Register handler called during SI polling.

  Arguments:    handler  Handler function

  Returns:      None
 *---------------------------------------------------------------------------*/
void SIRegisterPollingHandler(void (*handler)(void*, void*)) {
    (void)handler;
    // Polling handled by PAD module on PC
}

/*---------------------------------------------------------------------------*
  Name:         SIUnregisterPollingHandler

  Description:  Unregister polling handler.

  Arguments:    handler  Handler function

  Returns:      None
 *---------------------------------------------------------------------------*/
void SIUnregisterPollingHandler(void (*handler)(void*, void*)) {
    (void)handler;
    // Polling handled by PAD module on PC
}

/*---------------------------------------------------------------------------*
  Name:         SITransferCommands

  Description:  Execute queued SI commands.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void SITransferCommands(void) {
    // No-op on PC (PAD module handles commands)
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
    // Wireless handled by OS/SDL2 on PC
}
