#ifndef DOLPHIN_OS_H
#define DOLPHIN_OS_H

#include "dolphin/types.h"
#include "dolphin/os/OSTime.h"
#include "dolphin/os/OSContext.h"
#include "dolphin/os/OSThread.h"
#include "dolphin/os/OSMutex.h"
#include "dolphin/os/OSSemaphore.h"
#include "dolphin/os/OSAlarm.h"
#include "dolphin/os/OSMessage.h"
#include "dolphin/os/OSAlloc.h"
#include "dolphin/os/OSCache.h"
#include "dolphin/os/OSError.h"
#include "dolphin/os/OSFont.h"
#include "dolphin/os/OSInterrupt.h"
#include "dolphin/os/OSMemory.h"
#include "dolphin/os/OSReset.h"
#include "dolphin/os/OSResetSW.h"
#include "dolphin/os/OSRtc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Main OS Initialization */
void OSInit(void);

/* Console/System Info */
u32  OSGetConsoleType(void);

/* Debug Output */
void OSReport(const char* fmt, ...);
void OSPanic(const char* file, int line, const char* fmt, ...);

/* Arena Management */
void* OSGetArenaHi(void);
void* OSGetArenaLo(void);
void  OSSetArenaHi(void* addr);
void  OSSetArenaLo(void* addr);

void* OSGetMEM1ArenaHi(void);
void* OSGetMEM1ArenaLo(void);
void  OSSetMEM1ArenaHi(void* addr);
void  OSSetMEM1ArenaLo(void* addr);

void* OSGetMEM2ArenaHi(void);
void* OSGetMEM2ArenaLo(void);
void  OSSetMEM2ArenaHi(void* addr);
void  OSSetMEM2ArenaLo(void* addr);

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OS_H */
