#include <dolphin/base/PPCArch.h>
#include <dolphin/os.h>
#include <limits.h>
#include <stddef.h>
#ifdef LIBPORPOISE_PORT
#include <SDL2/SDL.h>
#include <dolphin/os/OSTime.h>
#endif

// forward declarations
static OSAlarmQueue AlarmQueue;

static void DecrementerExceptionHandler(__OSException exception, OSContext* context);


#ifdef LIBPORPOISE_PORT
#ifdef LIBPORPOISE_BUILD_WIN
static unsigned int OSSDLTimerCallback(unsigned int interval, void * param)
#else
static u32 OSSDLTimerCallback(u32 interval, void * param)
#endif
{
	OSAlarm * myAlarm = (OSAlarm*)param;


    if(myAlarm) {
		SDL_RemoveTimer(myAlarm->sdlTimer);

        if(myAlarm->period == 0) {
            myAlarm->sdlTimer = 0;
        } else {
			myAlarm->sdlTimer = SDL_AddTimer(OSTicksToMilliseconds(myAlarm->period), OSSDLTimerCallback, myAlarm);
		}
        if(myAlarm->handler) {
            myAlarm->handler(myAlarm, NULL);
        }
    }
    return 0;
}
#endif

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00013C
 */
int OSCheckAlarmQueue(void)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000098
 */
static void SetTimer(OSAlarm* alarm)
{
	OSTime delta;

#if OS_BUILD_VERSION >= 20011002L
	delta = alarm->fire - __OSGetSystemTime();
#else
	delta = alarm->fire - OSGetTime();
#endif

	if (delta < 0) {
		PPCMtdec(0);
	} else if (delta < (OSTime)INT_MAX + 1) {
		PPCMtdec(delta);
	} else {
		PPCMtdec(INT_MAX);
	}
}

/**
 * @TODO: Documentation
 */
void OSInitAlarm(void)
{
	if (__OSGetExceptionHandler(__OS_EXCEPTION_DECREMENTER) != DecrementerExceptionHandler) {
		AlarmQueue.head = AlarmQueue.tail = NULL;
		__OSSetExceptionHandler(__OS_EXCEPTION_DECREMENTER, DecrementerExceptionHandler);
	}
}

/**
 * @TODO: Documentation
 */
void OSCreateAlarm(OSAlarm* alarm)
{
	alarm->handler = NULL;
}

/**
 * @TODO: Documentation
 */
static void InsertAlarm(OSAlarm* alarm, OSTime fire, OSAlarmHandler handler)
{
	OSAlarm* next;
	OSAlarm* prev;

	if (alarm->period > 0) {
#if OS_BUILD_VERSION >= 20011002L
		OSTime time = __OSGetSystemTime();
#else
		OSTime time = OSGetTime();
#endif

		fire = alarm->start;
		if (alarm->start < time) {
			fire += alarm->period * ((time - alarm->start) / alarm->period + 1);
		}
	}

	alarm->handler = handler;
	alarm->fire    = fire;

	for (next = AlarmQueue.head; next; next = next->next) {
		if (next->fire <= fire) {
			continue;
		}

		alarm->prev = next->prev;
		next->prev  = alarm;
		alarm->next = next;
		prev        = alarm->prev;
		if (prev) {
			prev->next = alarm;
		} else {
			AlarmQueue.head = alarm;
			SetTimer(alarm);
		}
		return;
	}
	alarm->next     = NULL;
	prev            = AlarmQueue.tail;
	AlarmQueue.tail = alarm;
	alarm->prev     = prev;
	if (prev) {
		prev->next = alarm;
	} else {
		AlarmQueue.head = AlarmQueue.tail = alarm;
		SetTimer(alarm);
	}
}

/**
 * @TODO: Documentation
 */
void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler)
{
	BOOL enabled;
	enabled       = OSDisableInterrupts();
	alarm->period = 0;
#if OS_BUILD_VERSION >= 20011002L
	InsertAlarm(alarm, __OSGetSystemTime() + tick, handler);
#else
	InsertAlarm(alarm, OSGetTime() + tick, handler);
#endif
	OSRestoreInterrupts(enabled);

#ifdef LIBPORPOISE_PORT
	alarm->sdlTimer = SDL_AddTimer(OSTicksToMilliseconds(alarm->start), OSSDLTimerCallback, alarm);
#endif
}

/**
 * @TODO: Documentation
 */
void OSSetAbsAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler)
{
	int enabled;

	enabled       = OSDisableInterrupts();
	alarm->period = 0;
	InsertAlarm(alarm, tick, handler);
	OSRestoreInterrupts(enabled);
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000070
 */
void OSSetPeriodicAlarm(OSAlarm*, OSTime, OSTime, OSAlarmHandler)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 */
void OSCancelAlarm(OSAlarm* alarm)
{
	OSAlarm* next;
	BOOL enabled;

	enabled = OSDisableInterrupts();

	if (alarm->handler == NULL) {
		OSRestoreInterrupts(enabled);
		return;
	}

	next = alarm->next;
	if (next == NULL) {
		AlarmQueue.tail = alarm->prev;
	} else {
		next->prev = alarm->prev;
	}
	if (alarm->prev) {
		alarm->prev->next = next;
	} else {
		AlarmQueue.head = next;
		if (next) {
			SetTimer(next);
		}
	}
	alarm->handler = NULL;

	OSRestoreInterrupts(enabled);

#ifdef LIBPORPOISE_PORT
	SDL_RemoveTimer(alarm->sdlTimer);
	alarm->sdlTimer = 0;
#endif
}

/**
 * @TODO: Documentation
 */
static void DecrementerExceptionCallback(__OSException exception, OSContext* context)
{
	OSAlarm* alarm;
	OSAlarm* next;
	OSAlarmHandler handler;
	OSTime time;

#if OS_BUILD_VERSION >= 20011002L
	OSContext exceptionContext;
	time = __OSGetSystemTime();
#else
	time = OSGetTime();
#endif
	alarm = AlarmQueue.head;
	if (alarm == NULL) {
		OSLoadContext(context);
	}

	if (time < alarm->fire) {
		SetTimer(alarm);
		OSLoadContext(context);
	}

	next            = alarm->next;
	AlarmQueue.head = next;
	if (next == NULL) {
		AlarmQueue.tail = NULL;
	} else {
		next->prev = NULL;
	}

	handler        = alarm->handler;
	alarm->handler = NULL;
	if (0 < alarm->period) {
		InsertAlarm(alarm, 0, handler);
	}

	if (AlarmQueue.head) {
		SetTimer(AlarmQueue.head);
	}

	OSDisableScheduler();
#if OS_BUILD_VERSION >= 20011002L
	OSClearContext(&exceptionContext);
	OSSetCurrentContext(&exceptionContext);
#endif
	handler(alarm, context);
#if OS_BUILD_VERSION >= 20011002L
	OSClearContext(&exceptionContext);
	OSSetCurrentContext(context);
#endif
	OSEnableScheduler();
	__OSReschedule();
	OSLoadContext(context);
}

/**
 * @TODO: Documentation
 */
static ASM void DecrementerExceptionHandler(register __OSException exception, register OSContext* context)
{
#ifdef __MWERKS__ // clang-format off
	nofralloc
	OS_EXCEPTION_SAVE_GPRS(context)
	b  DecrementerExceptionCallback
#endif // clang-format on
}
