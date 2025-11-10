#ifndef DOLPHIN_OSRESET_H
#define DOLPHIN_OSRESET_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OS_RESETCODE_RESTART    0x80000000
#define OS_RESETCODE_SYSTEM     0x40000000
#define OS_RESETCODE_SWITCH     0x20000000
#define OS_RESETCODE_EXEC       0xC0000000
#define OS_RESETCODE_NETCONFIG  0xC0010000
#define OS_RESETCODE_LAUNCH     0xA0000000
#define OS_RESETCODE_INSTALLER  0xA0020000

#define OS_RESET_TIMEOUT        OSMillisecondsToTicks(1000)

#define OS_RESET_RESTART        0
#define OS_RESET_HOTRESET       1
#define OS_RESET_SHUTDOWN       2

#define OS_SHUTDOWN_PRIO_SO         110
#define OS_SHUTDOWN_PRIO_IP         111
#define OS_SHUTDOWN_PRIO_MIDI       126
#define OS_SHUTDOWN_PRIO_CARD       127
#define OS_SHUTDOWN_PRIO_PAD        127
#define OS_SHUTDOWN_PRIO_WPAD       127
#define OS_SHUTDOWN_PRIO_STEERING   127
#define OS_SHUTDOWN_PRIO_GX         127
#define OS_SHUTDOWN_PRIO_MEMPROT    127
#define OS_SHUTDOWN_PRIO_ALARM      4294967295
#define OS_SHUTDOWN_PRIO_VI         127
#define OS_SHUTDOWN_PRIO_NAND       255
#define OS_SHUTDOWN_PRIO_KBD        127

#define OS_SD_FATALERR          0
#define OS_SD_REBOOT            1
#define OS_SD_SHUTDOWN          2
#define OS_SD_IDLE              3
#define OS_SD_RESTART           4
#define OS_SD_RETURNTOMENU      5
#define OS_SD_EXEC              6
#define OS_SD_LAUNCH            7

typedef BOOL (*OSShutdownFunction)(BOOL final, u32 event);
typedef struct OSShutdownFunctionInfo OSShutdownFunctionInfo;

struct OSShutdownFunctionInfo
{
    OSShutdownFunction      func;
    u32                     priority;
    OSShutdownFunctionInfo* next;
    OSShutdownFunctionInfo* prev;
};

void OSRegisterShutdownFunction   (OSShutdownFunctionInfo* info);
BOOL OSRegisterResetFunction      (OSShutdownFunctionInfo* info);  // Alias for shutdown
void OSUnregisterShutdownFunction (OSShutdownFunctionInfo* info);
void OSRebootSystem               (void);
void OSShutdownSystem             (void);
void OSRestart                    (u32 resetCode);
void OSReturnToMenu               (void);
void OSReturnToDataManager        (void);
void OSResetSystem                (int reset, u32 resetCode, BOOL forceMenu);
u32  OSGetResetCode               (void);
void OSGetSaveRegion              (void** start, void** end);
void OSGetSavedRegion             (void** start, void** end);
void OSSetSaveRegion              (void* start, void* end);

// Internal functions
void __OSDoHotReset               (void);  // Perform hot reset
BOOL __DVDPrepareResetAsync       (void (*callback)(void));  // DVD reset prep

#define OSIsRestart() ((OSGetResetCode() & OS_RESETCODE_RESTART) ? TRUE : FALSE)

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OSRESET_H */

