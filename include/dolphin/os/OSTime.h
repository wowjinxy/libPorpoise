#ifndef DOLPHIN_OSTIME_H
#define DOLPHIN_OSTIME_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef s64 OSTime;
typedef u32 OSTick;

#define OS_TIMER_CLOCK      40500000  // 40.5 MHz
#define OS_BUS_CLOCK        162000000 // 162 MHz

// Time conversion macros
#define OSSecondsToTicks(sec)           ((OSTime)(sec) * OS_TIMER_CLOCK)
#define OSMillisecondsToTicks(msec)     ((OSTime)(msec) * (OS_TIMER_CLOCK / 1000))
#define OSMicrosecondsToTicks(usec)     ((OSTime)(usec) * (OS_TIMER_CLOCK / 1000000))
#define OSNanosecondsToTicks(nsec)      (((OSTime)(nsec) * (OS_TIMER_CLOCK / 1000000)) / 1000)

#define OSTicksToSeconds(ticks)         ((ticks) / OS_TIMER_CLOCK)
#define OSTicksToMilliseconds(ticks)    ((ticks) / (OS_TIMER_CLOCK / 1000))
#define OSTicksToMicroseconds(ticks)    ((ticks) / (OS_TIMER_CLOCK / 1000000))
#define OSTicksToNanoseconds(ticks)     (((ticks) * 1000) / (OS_TIMER_CLOCK / 1000000))

OSTime OSGetTime(void);
OSTick OSGetTick(void);
OSTime OSGetSystemTime(void);

void   OSTicksToCalendarTime(OSTime ticks, u16* year, u8* month, u8* day,
                             u8* hour, u8* minute, u8* second);
OSTime OSCalendarTimeToTicks(u16 year, u8 month, u8 day,
                             u8 hour, u8 minute, u8 second);

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OSTIME_H */

