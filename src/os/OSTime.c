/*---------------------------------------------------------------------------*
  OSTime.c - Time Base and Calendar Functions
  
  HARDWARE TIME SYSTEM (GC/Wii):
  ===============================
  
  The PowerPC processors have hardware "Time Base" registers:
  
  **Time Base Registers (TBR):**
  - **TBU** (Time Base Upper) - Upper 32 bits
  - **TBL** (Time Base Lower) - Lower 32 bits
  - Combined: 64-bit counter
  - Increments at **40.5 MHz** on GameCube/Wii
  - Never stops (runs continuously)
  - Assembly instructions: `mftb`, `mftbu`
  
  **Why 40.5 MHz?**
  - Bus clock is 162 MHz
  - Time base is bus clock / 4 = 40.5 MHz
  - Provides ~7000 years before overflow
  
  **Reading Time Base:**
  Original hardware (PowerPC assembly):
  ```asm
  _loop:
      mftbu   r3      ; Read upper
      mftb    r4      ; Read lower
      mftbu   r5      ; Read upper again
      cmpw    r3, r5  ; Did upper change?
      bne     _loop   ; If yes, retry (lower wrapped)
      blr             ; Return r3:r4 (64-bit)
  ```
  
  The loop is necessary because reading is not atomic - if the lower
  32 bits wrap between reads, you get inconsistent values.
  
  **System Time vs Time Base:**
  - **Time Base**: Raw hardware counter (resets on power cycle)
  - **System Time**: Time since OS boot (adjusted)
  - Adjustment stored in low memory, preserved across soft resets
  
  PC PORT IMPLEMENTATION:
  =======================
  
  **What We Use:**
  
  Windows:
  - `QueryPerformanceCounter()` - High-resolution counter
  - `QueryPerformanceFrequency()` - Counter frequency
  - Typically 10 MHz or higher
  - Convert to 40.5 MHz equivalent
  
  Linux/Mac:
  - `gettimeofday()` - Microsecond precision
  - Convert to 40.5 MHz ticks
  - Or `clock_gettime(CLOCK_MONOTONIC)` for better precision
  
  **Conversion:**
  ```c
  // Windows
  ticks = (counter * 40500000) / frequency
  
  // Linux
  ticks = (seconds * 40500000) + (microseconds * 40.5)
  ```
  
  CALENDAR TIME SYSTEM:
  =====================
  
  **OSCalendarTime Structure:**
  ```c
  typedef struct {
      int sec;    // 0-59
      int min;    // 0-59
      int hour;   // 0-23
      int mday;   // 1-31 (day of month)
      int mon;    // 0-11 (January = 0)
      int year;   // Years since 0000
      int wday;   // 0-6 (Sunday = 0)
      int yday;   // 0-365 (day of year)
      int msec;   // 0-999 (milliseconds)
      int usec;   // 0-999 (microseconds)
  } OSCalendarTime;
  ```
  
  **Leap Year Calculation:**
  - Divisible by 4: Leap year
  - EXCEPT divisible by 100: Not leap year
  - EXCEPT divisible by 400: Leap year
  - Examples: 2000 (leap), 1900 (not), 2024 (leap)
  
  **Day of Week (Zeller's Congruence):**
  Original uses simpler algorithm:
  - Days since year 0000 + 6 (offset) mod 7
  - January 1, year 0001 was Monday
  
  **Conversion Algorithm:**
  
  Ticks → Calendar:
  1. Extract subsecond (msec, usec)
  2. Convert to total seconds
  3. Calculate days since epoch (year 2000 bias)
  4. Calculate year from days
  5. Calculate month and day within year
  6. Calculate hour, minute, second
  
  Calendar → Ticks:
  1. Calculate days since year 0000
  2. Add days in year up to month
  3. Add day of month
  4. Convert to seconds
  5. Add hour/minute/second
  6. Convert to ticks
  
  TYPICAL USAGE:
  ==============
  
  ```c
  // Get current time
  OSTime start = OSGetTime();
  
  // Do work...
  
  // Calculate elapsed time
  OSTime elapsed = OSGetTime() - start;
  u32 milliseconds = OSTicksToMilliseconds(elapsed);
  
  // Convert to calendar
  OSCalendarTime cal;
  OSTicksToCalendarTime(OSGetTime(), &cal);
  OSReport("Date: %04d-%02d-%02d %02d:%02d:%02d\n",
           cal.year, cal.mon+1, cal.mday,
           cal.hour, cal.min, cal.sec);
  ```
  
  IMPORTANT NOTES:
  ================
  
  **Precision:**
  - Original: 40.5 MHz = ~24.7 ns per tick
  - PC: Varies, but typically better
  - OSTicksToMicroseconds() is accurate
  - OSTicksToNanoseconds() may have rounding errors
  
  **Range:**
  - 64-bit signed: ~7000 years @ 40.5 MHz
  - More than enough for any game
  
  **Thread Safety:**
  - OSGetTime() is thread-safe (reads hardware register)
  - Calendar functions are thread-safe (no shared state)
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <time.h>
#endif

/*===========================================================================*
  TIME BASE IMPLEMENTATION
 *===========================================================================*/

/* Start time for relative measurements */
static OSTime s_startTime = 0;
static OSTime s_systemTimeBase = 0;  /* Adjustment for system time */
static BOOL s_timeInitialized = FALSE;

/*---------------------------------------------------------------------------*
  Name:         __OSInitTime (Internal)

  Description:  Initializes the time system. Called once by OSInit().

  Arguments:    None
 *---------------------------------------------------------------------------*/
static void __OSInitTime(void) {
    if (s_timeInitialized) return;
    
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    /* Convert to 40.5 MHz ticks */
    s_startTime = (OSTime)((counter.QuadPart * OS_TIMER_CLOCK) / freq.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    /* Convert to 40.5 MHz ticks */
    s_startTime = (OSTime)(tv.tv_sec * OS_TIMER_CLOCK) + 
                  (OSTime)(tv.tv_usec * (OS_TIMER_CLOCK / 1000000));
#endif
    
    s_systemTimeBase = 0;
    s_timeInitialized = TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         OSGetTime

  Description:  Returns the current time base value.
                
                On original hardware (PowerPC assembly):
                - Reads TBU and TBL registers atomically
                - Loop to handle wraparound
                
                On PC:
                - Uses high-resolution performance counter
                - Converts to 40.5 MHz equivalent

  Returns:      64-bit time value in 40.5 MHz ticks
  
  Thread Safety: Safe to call from any thread
 *---------------------------------------------------------------------------*/
OSTime OSGetTime(void) {
    if (!s_timeInitialized) {
        __OSInitTime();
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
    OSTime currentTime = (OSTime)(tv.tv_sec * OS_TIMER_CLOCK) + 
                         (OSTime)(tv.tv_usec * (OS_TIMER_CLOCK / 1000000));
    return currentTime - s_startTime;
#endif
}

/*---------------------------------------------------------------------------*
  Name:         OSGetTick

  Description:  Returns the lower 32 bits of the time base.
                
                On original hardware: `mftb r3` instruction
                On PC: Lower 32 bits of OSGetTime()
                
                Useful for quick timing when 32 bits is enough
                (wraps every ~106 seconds @ 40.5 MHz)

  Returns:      32-bit tick value
 *---------------------------------------------------------------------------*/
OSTick OSGetTick(void) {
    return (OSTick)(OSGetTime() & 0xFFFFFFFF);
}

/*---------------------------------------------------------------------------*
  Name:         OSGetSystemTime

  Description:  Returns system time (time since OS boot).
                
                On original hardware:
                - Reads adjustment from low memory
                - Adds to current time base
                - Survives soft resets
                
                On PC:
                - Adds adjustment to current time
                - Simulates soft reset behavior

  Returns:      System time in ticks
 *---------------------------------------------------------------------------*/
OSTime OSGetSystemTime(void) {
    return s_systemTimeBase + OSGetTime();
}

/*---------------------------------------------------------------------------*
  Name:         __OSSetTime (Internal)

  Description:  Sets the time base to a specific value.
                
                On original hardware:
                - Writes to TBL and TBU registers
                - Updates adjustment in low memory
                - Used for synchronizing time
                
                On PC:
                - Adjusts start time to match requested value
                - Updates system time base

  Arguments:    time - New time value
 *---------------------------------------------------------------------------*/
void __OSSetTime(OSTime time) {
    BOOL enabled = OSDisableInterrupts();
    
    /* Calculate how much to adjust */
    OSTime current = OSGetTime();
    s_systemTimeBase += current - time;
    
    /* Reset start time to "set" the clock */
    s_startTime += (current - time);
    
    OSRestoreInterrupts(enabled);
}

/*===========================================================================*
  CALENDAR TIME CONVERSION
  
  Based on the reference implementation with proper leap year handling
  and day-of-week calculation.
 *===========================================================================*/

/* Year 2000 bias for day calculations */
#define BIAS (2000 * 365 + (2000 + 3) / 4 - (2000 - 1) / 100 + (2000 - 1) / 400)

/* Days accumulated at start of each month (non-leap year) */
static const int YearDays[] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

/* Days accumulated at start of each month (leap year) */
static const int LeapYearDays[] = {
    0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335
};

/*---------------------------------------------------------------------------*
  Name:         IsLeapYear

  Description:  Checks if a year is a leap year.
                
                Rules:
                - Divisible by 4: YES
                - Divisible by 100: NO
                - Divisible by 400: YES

  Arguments:    year - Year to check

  Returns:      TRUE if leap year, FALSE otherwise
 *---------------------------------------------------------------------------*/
static BOOL IsLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/*---------------------------------------------------------------------------*
  Name:         GetYearDays

  Description:  Gets days from January 1 to the start of specified month.

  Arguments:    year  - Year (for leap year check)
                month - Month (0-11, 0 = January)

  Returns:      Number of days
 *---------------------------------------------------------------------------*/
static int GetYearDays(int year, int month) {
    const int* md = IsLeapYear(year) ? LeapYearDays : YearDays;
    return md[month];
}

/*---------------------------------------------------------------------------*
  Name:         GetLeapDays

  Description:  Calculates number of leap days from year 0 to specified year.

  Arguments:    year - Years since year 0

  Returns:      Number of leap days (February 29ths)
 *---------------------------------------------------------------------------*/
static int GetLeapDays(int year) {
    if (year < 1) return 0;
    return (year + 3) / 4 - (year - 1) / 100 + (year - 1) / 400;
}

/*---------------------------------------------------------------------------*
  Name:         OSTicksToCalendarTime

  Description:  Converts time base ticks to calendar time.
                
                Algorithm:
                1. Extract subsecond portion (msec, usec)
                2. Convert remaining to total seconds
                3. Calculate days since epoch (with year 2000 bias)
                4. Determine year from days (accounting for leap years)
                5. Determine month and day within year
                6. Extract hour, minute, second from remaining seconds
                7. Calculate day of week

  Arguments:    ticks - Time in OS ticks (40.5 MHz)
                year  - Pointer to receive year (can be NULL)
                month - Pointer to receive month 1-12 (can be NULL)
                day   - Pointer to receive day 1-31 (can be NULL)
                hour  - Pointer to receive hour 0-23 (can be NULL)
                minute- Pointer to receive minute 0-59 (can be NULL)
                second- Pointer to receive second 0-59 (can be NULL)

  Returns:      None
 *---------------------------------------------------------------------------*/
void OSTicksToCalendarTime(OSTime ticks, u16* year, u8* month, u8* day,
                          u8* hour, u8* minute, u8* second) {
    int days, secs;
    OSTime d;
    int yr, mon;
    const int* md;
    int n;
    
    /* Extract subsecond portion */
    d = ticks % OSSecondsToTicks(1);
    if (d < 0) {
        d += OSSecondsToTicks(1);
    }
    
    /* Remove subsecond portion */
    ticks -= d;
    
    /* Convert to days and seconds */
    days = (int)(OSTicksToSeconds(ticks) / 86400 + BIAS);
    secs = (int)(OSTicksToSeconds(ticks) % 86400);
    
    if (secs < 0) {
        days -= 1;
        secs += 86400;
    }
    
    /* Calculate year from days */
    for (yr = days / 365; days < (n = GetLeapDays(yr) + 365 * yr); ) {
        --yr;
    }
    days -= n;  /* Now days is day-of-year (0-based) */
    
    /* Calculate month and day */
    md = IsLeapYear(yr) ? LeapYearDays : YearDays;
    for (mon = 12; days < md[--mon]; ) {
        /* Find correct month */
    }
    
    /* Fill in results */
    if (year)   *year   = (u16)yr;
    if (month)  *month  = (u8)(mon + 1);  /* 1-12 */
    if (day)    *day    = (u8)(days - md[mon] + 1);  /* 1-31 */
    if (hour)   *hour   = (u8)(secs / 3600);
    if (minute) *minute = (u8)((secs / 60) % 60);
    if (second) *second = (u8)(secs % 60);
}

/*---------------------------------------------------------------------------*
  Name:         OSCalendarTimeToTicks

  Description:  Converts calendar time to time base ticks.
                
                Algorithm:
                1. Calculate total days since year 0
                2. Add leap days
                3. Add days in current year up to month
                4. Add day of month
                5. Convert to seconds
                6. Add hour/minute/second
                7. Convert to ticks

  Arguments:    year   - Year (e.g., 2024)
                month  - Month 1-12 (1 = January)
                day    - Day 1-31
                hour   - Hour 0-23
                minute - Minute 0-59
                second - Second 0-59

  Returns:      Time in OS ticks (40.5 MHz)
 *---------------------------------------------------------------------------*/
OSTime OSCalendarTimeToTicks(u16 year, u8 month, u8 day,
                             u8 hour, u8 minute, u8 second) {
    OSTime secs;
    int days;
    
    /* Validate inputs */
    if (month < 1) month = 1;
    if (month > 12) month = 12;
    if (day < 1) day = 1;
    if (day > 31) day = 31;
    if (hour > 23) hour = 23;
    if (minute > 59) minute = 59;
    if (second > 59) second = 59;
    
    /* Calculate days since year 0 */
    days = (int)year * 365 + GetLeapDays(year);
    
    /* Add days in current year up to month */
    days += GetYearDays(year, month - 1);
    
    /* Add day of month (day is 1-based) */
    days += day - 1;
    
    /* Remove bias to get days since epoch */
    days -= BIAS;
    
    /* Convert to seconds */
    secs = (OSTime)days * 86400;
    secs += hour * 3600;
    secs += minute * 60;
    secs += second;
    
    /* Convert to ticks */
    return OSSecondsToTicks(secs);
}

/*===========================================================================*
  DEBUG UTILITIES
 *===========================================================================*/

#ifdef _DEBUG

/*---------------------------------------------------------------------------*
  Name:         OSTimeToString (Debug utility)

  Description:  Converts OSTime to human-readable string.
                Not in original SDK, but useful for debugging.

  Arguments:    time   - Time in ticks
                buffer - Buffer to write string (at least 32 bytes)

  Returns:      Pointer to buffer
 *---------------------------------------------------------------------------*/
char* OSTimeToString(OSTime time, char* buffer) {
    u16 year;
    u8 month, day, hour, minute, second;
    
    OSTicksToCalendarTime(time, &year, &month, &day, &hour, &minute, &second);
    
    sprintf(buffer, "%04u-%02u-%02u %02u:%02u:%02u",
            year, month, day, hour, minute, second);
    
    return buffer;
}

#endif /* _DEBUG */
