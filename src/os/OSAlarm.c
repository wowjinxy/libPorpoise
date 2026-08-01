#include <dolphin/base/PPCArch.h>
#include <dolphin/os.h>
#include <limits.h>
#include <stddef.h>
#ifdef LIBPORPOISE_PORT
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_thread.h>
#endif

// forward declarations
static OSAlarmQueue AlarmQueue;
#ifdef LIBPORPOISE_PORT
static SDL_threadID HostAlarmThread;
static BOOL HostAlarmDispatching;
#endif

static void DecrementerExceptionHandler(__OSException exception, OSContext* context);
static void InsertAlarm(OSAlarm* alarm, OSTime fire, OSAlarmHandler handler);

#ifdef LIBPORPOISE_PORT
void __OSHostRegisterAlarmThread(void)
{
	BOOL enabled = OSDisableInterrupts();

	if (HostAlarmThread == 0) {
		HostAlarmThread = SDL_ThreadID();
	}
	OSRestoreInterrupts(enabled);
}

BOOL __OSHostTransferAlarmThread(SDL_threadID expectedOwner)
{
	BOOL enabled = OSDisableInterrupts();
	BOOL transferred = FALSE;
	SDL_threadID currentThread = SDL_ThreadID();

	if (HostAlarmThread == expectedOwner) {
		if (HostAlarmThread != currentThread) {
			HostAlarmThread = currentThread;
			__OSHostClearAlarmWaitQueue();
		}
		transferred = TRUE;
	}
	OSRestoreInterrupts(enabled);
	return transferred;
}

BOOL __OSHostIsAlarmThread(void)
{
	BOOL enabled = OSDisableInterrupts();
	BOOL isAlarmThread;

	isAlarmThread =
	    HostAlarmThread != 0 &&
	    HostAlarmThread == SDL_ThreadID();
	OSRestoreInterrupts(enabled);
	return isAlarmThread;
}

u32 __OSHostGetAlarmTimeoutMilliseconds(void)
{
	OSAlarm* alarm;
	OSTime delta;
	OSTime ticksPerMillisecond;
	OSTime milliseconds;
	BOOL enabled;

	if (!__OSHostIsAlarmThread()) {
		return SDL_MUTEX_MAXWAIT;
	}

	enabled = OSDisableInterrupts();
	alarm = AlarmQueue.head;
	if (alarm == NULL) {
		OSRestoreInterrupts(enabled);
		return SDL_MUTEX_MAXWAIT;
	}

	delta = alarm->fire - __OSGetSystemTime();
	if (delta <= 0) {
		OSRestoreInterrupts(enabled);
		return 0;
	}

	ticksPerMillisecond = OS_TIMER_CLOCK / 1000;
	if (ticksPerMillisecond <= 0) {
		OSRestoreInterrupts(enabled);
		return 1;
	}
	milliseconds =
	    (delta + ticksPerMillisecond - 1) / ticksPerMillisecond;
	if (milliseconds >= SDL_MUTEX_MAXWAIT) {
		milliseconds = SDL_MUTEX_MAXWAIT - 1;
	}
	OSRestoreInterrupts(enabled);
	return (u32)milliseconds;
}
#endif

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00013C
 */
int OSCheckAlarmQueue(void)
{
#ifdef LIBPORPOISE_PORT
	BOOL handled = FALSE;
	BOOL enabled;

	enabled = OSDisableInterrupts();
	if (!__OSHostIsAlarmThread() || HostAlarmDispatching) {
		OSRestoreInterrupts(enabled);
		return FALSE;
	}
	HostAlarmDispatching = TRUE;
	for (;;) {
		OSAlarm* alarm;
		OSAlarmHandler handler;
		OSTime time;

		alarm = AlarmQueue.head;
		time = __OSGetSystemTime();
		if (alarm == NULL || time < alarm->fire) {
			break;
		}

		AlarmQueue.head = alarm->next;
		if (AlarmQueue.head == NULL) {
			AlarmQueue.tail = NULL;
		} else {
			AlarmQueue.head->prev = NULL;
		}

		handler = alarm->handler;
		alarm->handler = NULL;
		if (alarm->period > 0) {
			InsertAlarm(alarm, 0, handler);
		}

		if (handler != NULL) {
			handler(alarm, OSGetCurrentContext());
			handled = TRUE;
		}
	}

	HostAlarmDispatching = FALSE;
	OSRestoreInterrupts(enabled);
	return handled;
#else
	TRAP_UNIMPLEMENTED;
#endif
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
	BOOL enabled = OSDisableInterrupts();

	if (__OSGetExceptionHandler(__OS_EXCEPTION_DECREMENTER) != DecrementerExceptionHandler) {
		AlarmQueue.head = AlarmQueue.tail = NULL;
		__OSSetExceptionHandler(__OS_EXCEPTION_DECREMENTER, DecrementerExceptionHandler);
	}
	OSRestoreInterrupts(enabled);
}

/**
 * @TODO: Documentation
 */
void OSCreateAlarm(OSAlarm* alarm)
{
	alarm->handler = NULL;
	alarm->prev = NULL;
	alarm->next = NULL;
	alarm->period = 0;
	alarm->start = 0;
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
#ifdef LIBPORPOISE_PORT
		__OSHostWakeAlarmThread();
#endif
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
#ifdef LIBPORPOISE_PORT
	__OSHostWakeAlarmThread();
#endif
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
void OSSetPeriodicAlarm(
	OSAlarm* alarm,
	OSTime start,
	OSTime period,
	OSAlarmHandler handler)
{
	BOOL enabled;

	enabled = OSDisableInterrupts();
	alarm->period = period;
	alarm->start = start;
	InsertAlarm(alarm, 0, handler);
	OSRestoreInterrupts(enabled);
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
	alarm->prev = NULL;
	alarm->next = NULL;
#ifdef LIBPORPOISE_PORT
	__OSHostWakeAlarmThread();
#endif

	OSRestoreInterrupts(enabled);
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
