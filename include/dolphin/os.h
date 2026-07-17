#ifndef _DOLPHIN_OS_H
#define _DOLPHIN_OS_H

#include <dolphin/types.h>

#include "dolphin/OS/OSAlarm.h"
#include "dolphin/OS/OSAlloc.h"
#include "dolphin/OS/OSArena.h"
#include "dolphin/OS/OSBootInfo.h"
#include "dolphin/OS/OSCache.h"
#include "dolphin/OS/OSContext.h"
#include "dolphin/OS/OSError.h"
#include "dolphin/OS/OSException.h"
#include "dolphin/OS/OSExpansion.h"
#include "dolphin/OS/OSFastCast.h"
#include "dolphin/OS/OSFont.h"
#include "dolphin/OS/OSInterrupt.h"
#include "dolphin/OS/OSMemory.h"
#include "dolphin/OS/OSMessage.h"
#include "dolphin/OS/OSModule.h"
#include "dolphin/OS/OSMutex.h"
#include "dolphin/OS/OSReset.h"
#include "dolphin/OS/OSRtc.h"
#include "dolphin/OS/OSThread.h"
#include "dolphin/OS/OSTime.h"
#include "dolphin/OS/OSUtil.h"
#include "dolphin/OS/OSVersion.h"

BEGIN_SCOPE_EXTERN_C

/////////////// OS FUNCTIONS //////////////////////////////////////////////////////////////////////

// Initialisers.

extern void __OSPSInit();
extern void __OSFPRInit();
extern void __OSCacheInit();
extern void __OSContextInit();
extern void __OSInterruptInit();
extern void __OSThreadInit();
extern void __OSInitSystemCall();
extern void __OSModuleInit();
extern void __OSInitAudioSystem();
extern void __OSStopAudioSystem();
extern void __OSInitMemoryProtection();

void OSInit(void);

// Other OS functions.

#define OS_CONSOLE_RETAIL4     0x00000004
#define OS_CONSOLE_RETAIL3     0x00000003
#define OS_CONSOLE_RETAIL2     0x00000002
#define OS_CONSOLE_RETAIL1     0x00000001
#define OS_CONSOLE_RETAIL      0x00000000
#define OS_CONSOLE_DEVHW4      0x10000007
#define OS_CONSOLE_DEVHW3      0x10000006
#define OS_CONSOLE_DEVHW2      0x10000005
#define OS_CONSOLE_DEVHW1      0x10000004
#define OS_CONSOLE_MINNOW      0x10000003
#define OS_CONSOLE_ARTHUR      0x10000002
#define OS_CONSOLE_PC_EMULATOR 0x10000001
#define OS_CONSOLE_EMULATOR    0x10000000
#define OS_CONSOLE_DEVELOPMENT 0x10000000
#define OS_CONSOLE_DEVKIT      0x10000000
#define OS_CONSOLE_TDEVKIT     0x20000000

u32 OSGetConsoleType();

///////////////////////////////////////////////////////////////////////////////////////////////////

/////////////// Extern Things /////////////////////////////////////////////////////////////////////

extern BOOL __OSInIPL;
extern OSTime __OSStartTime;

///////////////////////////////////////////////////////////////////////////////////////////////////

END_SCOPE_EXTERN_C

#endif
