#ifndef DOLPHIN_OSSTOPWATCH_H
#define DOLPHIN_OSSTOPWATCH_H

#include <dolphin/types.h>
#include <dolphin/os/OSTime.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSStopwatch {
    char* name;
    OSTime total;
    OSTime min;
    OSTime max;
    OSTime last;
    OSTime start;
    u32 hits;
    BOOL running;
} OSStopwatch;

void OSInitStopwatch(OSStopwatch* sw, char* name);
void OSStartStopwatch(OSStopwatch* sw);
void OSStopStopwatch(OSStopwatch* sw);
OSTime OSCheckStopwatch(OSStopwatch* sw);
void OSResetStopwatch(OSStopwatch* sw);
void OSDumpStopwatch(OSStopwatch* sw);

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OSSTOPWATCH_H */

