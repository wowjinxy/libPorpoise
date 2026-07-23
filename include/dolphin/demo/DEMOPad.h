/*---------------------------------------------------------------------------*
  Project:  libPorpoise Demo Library
  File:     DEMOPad.h
  
  Controller input handling for demo framework.
  
  Based on Nintendo's Revolution SDK demo library.
 *---------------------------------------------------------------------------*/

#ifndef DOLPHIN_DEMO_PAD_H
#define DOLPHIN_DEMO_PAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dolphin/types.h>
#include <dolphin/pad.h>

/*---------------------------------------------------------------------------*
    DEMOPad.c
 *---------------------------------------------------------------------------*/
// used to detect which direction is the stick(s) pointing
#define DEMO_STICK_THRESHOLD      48

#define DEMO_STICK_UP             0x1000
#define DEMO_STICK_DOWN           0x2000
#define DEMO_STICK_LEFT           0x4000
#define DEMO_STICK_RIGHT          0x8000
#define DEMO_SUBSTICK_UP          0x0100
#define DEMO_SUBSTICK_DOWN        0x0200
#define DEMO_SUBSTICK_LEFT        0x0400
#define DEMO_SUBSTICK_RIGHT       0x0800

// extended pad status structure
typedef struct
{
    // contains PADStatus structure
    PADStatus   pst;

    // extended field
    u16         buttonDown;
    u16         buttonUp;
    u16         dirs;
    u16         dirsNew;
    u16         dirsReleased;
    s16         stickDeltaX;
    s16         stickDeltaY;
    s16         substickDeltaX;
    s16         substickDeltaY;
} DEMOPadStatus;

// the entity which keeps current pad status
extern DEMOPadStatus    DemoPad[PAD_MAX_CONTROLLERS];
extern u32              DemoNumValidPads;

// main function prototypes
extern void     DEMOPadInit( void );
extern void     DEMOPadRead( void );

// inline functions for getting each component
static inline u16 DEMOPadGetButton(u32 i)
    { return DemoPad[i].pst.button; }

static inline u16 DEMOPadGetButtonUp(u32 i)
    { return DemoPad[i].buttonUp; }

static inline u16 DEMOPadGetButtonDown(u32 i)
    { return DemoPad[i].buttonDown; }

static inline u16 DEMOPadGetDirs(u32 i)
    { return DemoPad[i].dirs; }

static inline u16 DEMOPadGetDirsNew(u32 i)
    { return DemoPad[i].dirsNew; }

static inline u16 DEMOPadGetDirsReleased(u32 i)
    { return DemoPad[i].dirsReleased; }

static inline s8  DEMOPadGetStickX(u32 i)
    { return DemoPad[i].pst.stickX; }

static inline s8  DEMOPadGetStickY(u32 i)
    { return DemoPad[i].pst.stickY; }

static inline s8  DEMOPadGetSubStickX(u32 i)
    { return DemoPad[i].pst.substickX; }

static inline s8  DEMOPadGetSubStickY(u32 i)
    { return DemoPad[i].pst.substickY; }

static inline u8  DEMOPadGetTriggerL(u32 i)
    { return DemoPad[i].pst.triggerLeft; }

static inline u8  DEMOPadGetTriggerR(u32 i)
    { return DemoPad[i].pst.triggerRight; }

static inline u8  DEMOPadGetAnalogA(u32 i)
    { return DemoPad[i].pst.analogA; }

static inline u8  DEMOPadGetAnalogB(u32 i)
    { return DemoPad[i].pst.analogB; }

static inline s8  DEMOPadGetErr(u32 i)
    { return DemoPad[i].pst.err; }

/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_DEMO_PAD_H */

