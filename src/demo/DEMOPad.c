#include <demo.h>
#include <dolphin.h>

DEMOPadStatus DemoPad[PAD_MAX_CONTROLLERS];
u32 DemoNumValidPads;

static PADStatus Pad[PAD_MAX_CONTROLLERS];

static u32 PadChanMask[PAD_MAX_CONTROLLERS] = {PAD_CHAN0_BIT, PAD_CHAN1_BIT,
                                               PAD_CHAN2_BIT, PAD_CHAN3_BIT};

static void DEMOPadCopy(PADStatus *pad, DEMOPadStatus *dmpad) {
  u16 dirs;

  if (pad->err != PAD_ERR_TRANSFER) {

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

    dmpad->dirsNew = PADButtonDown(dmpad->dirs, dirs);
    dmpad->dirsReleased = PADButtonUp(dmpad->dirs, dirs);
    dmpad->dirs = dirs;

    dmpad->buttonDown = PADButtonDown(dmpad->pst.button, pad->button);
    dmpad->buttonUp = PADButtonUp(dmpad->pst.button, pad->button);

    dmpad->stickDeltaX = (s16)(pad->stickX - dmpad->pst.stickX);
    dmpad->stickDeltaY = (s16)(pad->stickY - dmpad->pst.stickY);
    dmpad->substickDeltaX = (s16)(pad->substickX - dmpad->pst.substickX);
    dmpad->substickDeltaY = (s16)(pad->substickY - dmpad->pst.substickY);

    dmpad->pst = *pad;
  } else {

    dmpad->dirsNew = dmpad->dirsReleased = 0;

    dmpad->buttonDown = dmpad->buttonUp = 0;

    dmpad->stickDeltaX = dmpad->stickDeltaY = 0;
    dmpad->substickDeltaX = dmpad->substickDeltaY = 0;
  }
}

void DEMOPadRead(void) {
  s32 i;
  u32 ResetReq = 0;

  PADRead(Pad);

  PADClamp(Pad);

  DemoNumValidPads = 0;
  for (i = 0; i < PAD_MAX_CONTROLLERS; i++) {

    if (Pad[i].err == PAD_ERR_NONE || Pad[i].err == PAD_ERR_TRANSFER) {
      ++DemoNumValidPads;
    } else if (Pad[i].err == PAD_ERR_NO_CONTROLLER) {
      ResetReq |= PadChanMask[i];
    }

    DEMOPadCopy(&Pad[i], &DemoPad[i]);
  }

  if (ResetReq) {

    PADReset(ResetReq);
  }

  return;
}

void DEMOPadInit(void) {
  s32 i;

  PADInit();

  for (i = 0; i < PAD_MAX_CONTROLLERS; i++) {
    DemoPad[i].pst.button = 0;
    DemoPad[i].pst.stickX = 0;
    DemoPad[i].pst.stickY = 0;
    DemoPad[i].pst.substickX = 0;
    DemoPad[i].pst.substickY = 0;
    DemoPad[i].pst.triggerLeft = 0;
    DemoPad[i].pst.triggerRight = 0;
    DemoPad[i].pst.analogA = 0;
    DemoPad[i].pst.analogB = 0;
    DemoPad[i].pst.err = 0;
    DemoPad[i].buttonDown = 0;
    DemoPad[i].buttonUp = 0;
    DemoPad[i].dirs = 0;
    DemoPad[i].dirsNew = 0;
    DemoPad[i].dirsReleased = 0;
    DemoPad[i].stickDeltaX = 0;
    DemoPad[i].stickDeltaY = 0;
    DemoPad[i].substickDeltaX = 0;
    DemoPad[i].substickDeltaY = 0;
  }
}