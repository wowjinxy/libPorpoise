/*---------------------------------------------------------------------------*
  Project:  libPorpoise Demo Library
  File:     DEMOPad.c
  
  Controller input handling - forwards to libPorpoise PAD functions.
  
  Based on Nintendo's Revolution SDK demo library.
 *---------------------------------------------------------------------------*/

#include <dolphin/demo.h>
#include <dolphin/pad.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*
   Global Variables
 *---------------------------------------------------------------------------*/
DEMOPadStatus       DemoPad[PAD_MAX_CONTROLLERS];
u32                 DemoNumValidPads;

/*---------------------------------------------------------------------------*
   Local Variables
 *---------------------------------------------------------------------------*/
static PADStatus    Pad[PAD_MAX_CONTROLLERS]; // internal use only

/*---------------------------------------------------------------------------*/
static u32 PadChanMask[PAD_MAX_CONTROLLERS] =
{
    PAD_CHAN0_BIT, PAD_CHAN1_BIT, PAD_CHAN2_BIT, PAD_CHAN3_BIT
};

/*---------------------------------------------------------------------------*
    Name:           DEMOPadCopy

    Description:    This function copies informations of PADStatus into
                    DEMOPadStatus structure. Also attaches some extra
                    informations such as down/up, stick direction.
                    This function is internal use only.

                    Keeps previous state if PAD_ERR_TRANSFER is returned.

    Arguments:      pad   : copy source. (PADStatus)
                    dmpad : copy destination. (DEMOPadStatus)

    Returns:        None
 *---------------------------------------------------------------------------*/
static void DEMOPadCopy(PADStatus* pad, DEMOPadStatus* dmpad)
{
    u16  dirs;

    if (pad->err != PAD_ERR_TRANSFER)
    {
        // Detects which direction is the stick(s) pointing.
        dirs = 0;
        if (pad->stickX < -DEMO_STICK_THRESHOLD)
            dirs |= DEMO_STICK_LEFT;
        if (pad->stickX > DEMO_STICK_THRESHOLD)
            dirs |= DEMO_STICK_RIGHT;
        if (pad->stickY < -DEMO_STICK_THRESHOLD)
            dirs |= DEMO_STICK_DOWN;
        if (pad->stickY > DEMO_STICK_THRESHOLD)
            dirs |= DEMO_STICK_UP;
        if (pad->substickX < -DEMO_STICK_THRESHOLD)
            dirs |= DEMO_SUBSTICK_LEFT;
        if (pad->substickX > DEMO_STICK_THRESHOLD)
            dirs |= DEMO_SUBSTICK_RIGHT;
        if (pad->substickY < -DEMO_STICK_THRESHOLD)
            dirs |= DEMO_SUBSTICK_DOWN;
        if (pad->substickY > DEMO_STICK_THRESHOLD)
            dirs |= DEMO_SUBSTICK_UP;

        // Get the direction newly detected / released
        dmpad->dirsNew = (dmpad->dirs ^ dirs) & dirs;
        dmpad->dirsReleased = (dmpad->dirs ^ dirs) & dmpad->dirs;
        dmpad->dirs = dirs;

        // Get DOWN/UP status of all buttons
        dmpad->buttonDown = (dmpad->pst.button ^ pad->button) & pad->button;
        dmpad->buttonUp = (dmpad->pst.button ^ pad->button) & dmpad->pst.button;

        // Get delta of analogs
        dmpad->stickDeltaX = (s16)(pad->stickX - dmpad->pst.stickX);
        dmpad->stickDeltaY = (s16)(pad->stickY - dmpad->pst.stickY);
        dmpad->substickDeltaX = (s16)(pad->substickX - dmpad->pst.substickX);
        dmpad->substickDeltaY = (s16)(pad->substickY - dmpad->pst.substickY);

        // Copy current status into DEMOPadStatus field
        dmpad->pst = *pad;
    }
    else
    {
        // Get the direction newly detected / released
        dmpad->dirsNew = dmpad->dirsReleased = 0;

        // Get DOWN/UP status of all buttons
        dmpad->buttonDown = dmpad->buttonUp = 0;

        // Get delta of analogs
        dmpad->stickDeltaX = dmpad->stickDeltaY = 0;
        dmpad->substickDeltaX = dmpad->substickDeltaY = 0;
    }
}

/*---------------------------------------------------------------------------*
    Name:           DEMOPadRead

    Description:    Calls PADRead() and perform clamping. Get information
                    of button down/up and sets them into extended field.
                    This function also checks whether controllers are
                    actually connected.

    Arguments:      None

    Returns:        None
 *---------------------------------------------------------------------------*/
void DEMOPadRead(void)
{
    s32         i;
    u32         ResetReq = 0; // for error handling

    // Read current PAD status - forward to libPorpoise
    PADRead(Pad);

    // Clamp analog inputs - forward to libPorpoise
    PADClamp(Pad);

    DemoNumValidPads = 0;
    for (i = 0; i < PAD_MAX_CONTROLLERS; i++)
    {
        // Connection check
        if (Pad[i].err == PAD_ERR_NONE ||
            Pad[i].err == PAD_ERR_TRANSFER)
        {
            ++DemoNumValidPads;
        }
        else if (Pad[i].err == PAD_ERR_NO_CONTROLLER)
        {
            ResetReq |= PadChanMask[i];
        }

        DEMOPadCopy(&Pad[i], &DemoPad[i]);
    }

    // Try resetting pad channels which have been not valid
    if (ResetReq)
    {
        // Don't care return status
        PADReset(ResetReq);
    }
}

/*---------------------------------------------------------------------------*
    Name:           DEMOPadInit

    Description:    Initializes PAD library. This function should be called
                    before any other DEMOPad functions.

    Arguments:      None

    Returns:        None
 *---------------------------------------------------------------------------*/
void DEMOPadInit(void)
{
    // Forward to libPorpoise PAD initialization
    PADInit();
    
    // Initialize demo pad structures
    memset(DemoPad, 0, sizeof(DemoPad));
    DemoNumValidPads = 0;
}

#ifdef __cplusplus
} // extern "C"
#endif

