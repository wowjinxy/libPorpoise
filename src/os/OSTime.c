#include <dolphin/os/OSTime.h>

#include <dolphin/os.h>

#ifdef LIBPORPOISE_PORT
#include <sys/time.h>
#include <SDL2/SDL.h>
#endif

#define OS_TIME_MONTH_MAX    12
#define OS_TIME_WEEK_DAY_MAX 7
#define OS_TIME_YEAR_DAY_MAX 365
#define BIAS                 (2000 * 365 + (2000 + 3) / 4 - (2000 - 1) / 100 + (2000 - 1) / 400)

// End of each month in standard year
static s32 YearDays[OS_TIME_MONTH_MAX] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
// End of each month in leap year
static s32 LeapYearDays[OS_TIME_MONTH_MAX] = { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 };

/**
 * Returns the number of ticks since 00:00, 1 Jan 2000
 */
ASM OSTime OSGetTime(void) {
#ifdef __MWERKS__ // clang-format off
	nofralloc

	mftbu  r3
	mftb   r4

	// Check for possible carry from TBL to TBU
	mftbu  r5
	cmpw   r3, r5
	bne    OSGetTime

	blr
#endif // clang-format on
#ifdef LIBPORPOISE_PORT
	u64 gamecubeEpochStartTicks = OSSecondsToTicks(946684800); /* 1 Jan, 2000 00:00:00*/
	
	struct timeval nowTv;
    gettimeofday(&nowTv, NULL);
    
    u64 nowTicks = OSMicrosecondsToTicks((u64)nowTv.tv_sec * 1000000 + (u64)nowTv.tv_usec);
	return (OSTime)(nowTicks - gamecubeEpochStartTicks);
#endif
}

/**
 * @TODO: Documentation
 */
ASM u32 OSGetTick(void)
{
#ifdef __MWERKS__ // clang-format off
	nofralloc

	mftb  r3
	blr
#endif // clang-format on
#ifdef LIBPORPOISE_PORT
	return OSMillisecondsToTicks(SDL_GetTicks());
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000014
 */
void __SetTime(void)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000084
 */
void __OSSetTime(void)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 */
OSTime __OSGetSystemTime(void)
{
	BOOL enabled;
	OSTime* timeAdjustAddr = (OSTime*)(OSPhysicalToCached(0x30D8));
	OSTime result;

	enabled = OSDisableInterrupts();
	#ifdef LIBPORPOISE_PORT
	result = OSGetTime();
	#else
	result  = *timeAdjustAddr + OSGetTime();
	#endif
	OSRestoreInterrupts(enabled);

	return result;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000008 (Matching by size)
 */
ASM void __OSSetTick(register u32 tick)
{
#ifdef __MWERKS__ // clang-format off
	nofralloc
	mttbl  tick  // An educated guess
	blr
#endif // clang-format on
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000088 (Matching by size)
 */
static BOOL IsLeapYear(s32 year)
{
	return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 0000A8
 */
void GetYearDays(void)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000050 (Matching by size)
 */
static s32 GetLeapDays(s32 year)
{
	if (year < 1) {
		return 0;
	}
	return (year + 3) / 4 - (year - 1) / 100 + (year - 1) / 400;
}

/**
 * @TODO: Documentation
 */
static void GetDates(s32 days, OSCalendarTime* cal)
{
	s32 year;
	s32 totalDays;
	s32* p_days;
	s32 month;
	cal->wday = (days + 6) % OS_TIME_WEEK_DAY_MAX;

	for (year = days / OS_TIME_YEAR_DAY_MAX; days < (totalDays = year * OS_TIME_YEAR_DAY_MAX + GetLeapDays(year));) {
		year--;
	}

	days -= totalDays;
	cal->year = year;
	cal->yday = days;

	p_days = IsLeapYear(year) ? LeapYearDays : YearDays;
	month  = OS_TIME_MONTH_MAX;
	while (days < p_days[--month]) {
		;
	}
	cal->mon  = month;
	cal->mday = days - p_days[month] + 1;
}

#pragma dont_inline on

/**
 * @TODO: Documentation
 */
void OSTicksToCalendarTime(OSTime ticks, OSCalendarTime* cal)
{
	int days;
	int secs;
	OSTime d;

	d = ticks % OSSecondsToTicks(1);
	if (d < 0) {
		d += OSSecondsToTicks(1);
	}
	cal->usec = (int)(OSTicksToMicroseconds(d) % 1000);
	cal->msec = (int)(OSTicksToMilliseconds(d) % 1000);

	ticks -= d;
	days = (int)(OSTicksToSeconds(ticks) / 86400 + BIAS);
	secs = (int)(OSTicksToSeconds(ticks) % 86400);
	if (secs < 0) {
		days -= 1;
		secs += 24 * 60 * 60;
	}

	GetDates(days, cal);

	cal->hour = secs / 60 / 60;
	cal->min  = (secs / 60) % 60;
	cal->sec  = secs % 60;
}

#pragma dont_inline reset

/**
 * @TODO: Documentation
 * @note UNUSED Size: 0002E0
 */
OSTime OSCalendarTimeToTicks(OSCalendarTime*)
{
	/*
    OSTime secs;
    int ov_mon;
    int mon;
    int year;

    ov_mon = td->mon / MONTH_MAX;
    mon = td->mon - (ov_mon * MONTH_MAX);

    if (mon < 0) {
        mon += MONTH_MAX;
        ov_mon--;
    }

    ASSERTLINE(412, (ov_mon <= 0 && 0 <= td->year + ov_mon) || (0 < ov_mon && td->year <= INT_MAX - ov_mon));
    
    year = td->year + ov_mon;

    secs = (OSTime)SECS_IN_YEAR * year +
           (OSTime)SECS_IN_DAY * (GetLeapDays(year) + GetYearDays(year, mon) + td->mday - 1) +
           (OSTime)SECS_IN_HOUR * td->hour +
           (OSTime)SECS_IN_MIN * td->min +
           td->sec -
           (OSTime)0xEB1E1BF80ULL;

    return OSSecondsToTicks(secs) + OSMillisecondsToTicks((OSTime)td->msec) +
           OSMicrosecondsToTicks((OSTime)td->usec);
	*/
	return 0;
}
