#ifndef _DOLPHIN_OS_H
#define _DOLPHIN_OS_H

#include <dolphin/types.h>

#include <dolphin/os/OSAlarm.h>
#include <dolphin/os/OSAlloc.h>
#include <dolphin/os/OSArena.h>
#include <dolphin/os/OSBootInfo.h>
#include <dolphin/os/OSCache.h>
#include <dolphin/os/OSContext.h>
#include <dolphin/os/OSError.h>
#include <dolphin/os/OSException.h>
#include <dolphin/os/OSExpansion.h>
#include <dolphin/os/OSFastCast.h>
#include <dolphin/os/OSFont.h>
#include <dolphin/os/OSInterrupt.h>
#include <dolphin/os/OSMemory.h>
#include <dolphin/os/OSMessage.h>
#include <dolphin/os/OSModule.h>
#include <dolphin/os/OSMutex.h>
#include <dolphin/os/OSReset.h>
#include <dolphin/os/OSRtc.h>
#include <dolphin/os/OSStopwatch.h>
#include <dolphin/os/OSThread.h>
#include <dolphin/os/OSTime.h>
#include <dolphin/os/OSUtil.h>
#include <dolphin/os/OSVersion.h>

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
