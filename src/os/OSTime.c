#include <dolphin/os.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

static OSTime s_startTime = 0;
static int s_timeInitialized = 0;

static void InitTime(void) {
    if (s_timeInitialized) return;
    
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    s_startTime = (OSTime)((counter.QuadPart * OS_TIMER_CLOCK) / freq.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    s_startTime = (OSTime)(tv.tv_sec * OS_TIMER_CLOCK + tv.tv_usec * (OS_TIMER_CLOCK / 1000000));
#endif
    
    s_timeInitialized = 1;
}

OSTime OSGetTime(void) {
    if (!s_timeInitialized) {
        InitTime();
    }
    
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    OSTime currentTime = (OSTime)((counter.QuadPart * OS_TIMER_CLOCK) / freq.QuadPart);
    return currentTime - s_startTime;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    OSTime currentTime = (OSTime)(tv.tv_sec * OS_TIMER_CLOCK + tv.tv_usec * (OS_TIMER_CLOCK / 1000000));
    return currentTime - s_startTime;
#endif
}

OSTick OSGetTick(void) {
    return (OSTick)(OSGetTime() & 0xFFFFFFFF);
}

OSTime OSGetSystemTime(void) {
    return OSGetTime();
}

void OSTicksToCalendarTime(OSTime ticks, u16* year, u8* month, u8* day,
                          u8* hour, u8* minute, u8* second) {
    time_t seconds = (time_t)(ticks / OS_TIMER_CLOCK);
    struct tm* timeinfo = localtime(&seconds);
    
    if (year)   *year   = (u16)(1900 + timeinfo->tm_year);
    if (month)  *month  = (u8)(timeinfo->tm_mon + 1);
    if (day)    *day    = (u8)timeinfo->tm_mday;
    if (hour)   *hour   = (u8)timeinfo->tm_hour;
    if (minute) *minute = (u8)timeinfo->tm_min;
    if (second) *second = (u8)timeinfo->tm_sec;
}

OSTime OSCalendarTimeToTicks(u16 year, u8 month, u8 day,
                             u8 hour, u8 minute, u8 second) {
    struct tm timeinfo = {0};
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;
    
    time_t seconds = mktime(&timeinfo);
    return (OSTime)(seconds * OS_TIMER_CLOCK);
}
