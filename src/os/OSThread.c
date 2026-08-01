#include "dolphin/os/OSThread.h"
#include <dolphin/hw_regs.h>
#include <dolphin/os.h>
#ifndef LIBPORPOISE_PORT
#include "PowerPC_EABI_Support/Runtime/__ppc_eabi_linker.h"
#endif
#include <stddef.h>

static vu32 RunQueueBits;
static volatile BOOL RunQueueHint;
static vs32 Reschedule;

static OSThreadQueue RunQueue[32];
static OSThread IdleThread;
static OSThread DefaultThread;
static OSContext IdleContext;
static void DefaultSwitchThreadCallback(OSThread* from, OSThread* to);
#ifdef LIBPORPOISE_PORT
#define OS_HOST_THREAD_EXIT_CALLBACK_CAPACITY 8

static __thread OSThread* HostCurrentThread;
static __thread BOOL HostBlockingWaitSafePoint;
static __thread BOOL HostThreadExitNotified;
static OSThreadQueue* HostAlarmWaitQueue;
static OSHostThreadExitCallback
	HostThreadExitCallbacks[OS_HOST_THREAD_EXIT_CALLBACK_CAPACITY];
#endif

// Fabricated helper inlines.
// Initialise mutex queue (mutex equiv. to OSInitThreadQueue below).
static inline void InitMutexQueue(OSMutexQueue* queue)
{
	queue->head = queue->tail = NULL;
}

/**
 * @TODO: Documentation
 */
void __OSThreadInit(void)
{
	OSThread* thread = &DefaultThread;
	int prio;

	thread->state    = OS_THREAD_STATE_RUNNING;
	thread->attr     = OS_THREAD_ATTR_DETACH;
	thread->priority = thread->base = 16;
	thread->suspend                 = 0;
	thread->val                     = (void*)-1;
	thread->mutex                   = NULL;
	OSInitThreadQueue(&thread->queueJoin);
	InitMutexQueue(&thread->queueMutex);

	__OSFPUContext = &thread->context;
#ifdef LIBPORPOISE_PORT
	HostCurrentThread = thread;
	__OSHostRegisterAlarmThread();
	thread->stackBase = NULL;
	thread->stackEnd = &thread->hostStackMagic;
	thread->hostStackMagic = OS_THREAD_STACK_MAGIC;
#endif

	OSClearContext(&thread->context);
	OSSetCurrentContext(&thread->context);
	#ifndef LIBPORPOISE_PORT
	//TODO
	thread->stackBase   = (void*)_stack_addr;
	thread->stackEnd    = (void*)_stack_end;
	*(thread->stackEnd) = OS_THREAD_STACK_MAGIC;
	#endif

	RunQueueBits      = 0;
	__OSCurrentThread = thread;
	RunQueueHint      = FALSE;
	for (prio = OS_PRIORITY_MIN; prio <= OS_PRIORITY_MAX; ++prio) {
		OSInitThreadQueue(&RunQueue[prio]);
	}

	OSInitThreadQueue(&__OSActiveThreadQueue);
	AddTail(&__OSActiveThreadQueue, thread, linkActive);
	OSClearContext(&IdleContext);
	Reschedule = 0;
}

/**
 * @TODO: Documentation
 */
void OSInitThreadQueue(OSThreadQueue* threadQueue)
{
	threadQueue->head = threadQueue->tail = NULL;
#ifdef LIBPORPOISE_PORT
	threadQueue->hostMutex = SDL_CreateMutex();
	threadQueue->hostCondition = SDL_CreateCond();
	threadQueue->hostWakeGeneration = 0;
#endif
}

/**
 * @TODO: Documentation
 */
OSThread* OSGetCurrentThread(void)
{
#ifdef LIBPORPOISE_PORT
	return HostCurrentThread;
#else
	return __OSCurrentThread;
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000040 (Matching by size)
 */
static void __OSSwitchThread(OSThread* nextThread)
{
	__OSCurrentThread = nextThread;
	OSSetCurrentContext(&nextThread->context);
	OSLoadContext(&nextThread->context);
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00001C
 */
BOOL OSIsThreadSuspended(OSThread* thread)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 */
BOOL OSIsThreadTerminated(OSThread* thread)
{
	return (thread->state == OS_THREAD_STATE_MORIBUND || thread->state == OS_THREAD_STATE_NULL) ? TRUE : FALSE;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000044 (Matching by size)
 */
static BOOL __OSIsThreadActive(OSThread* thread)
{
	OSThread* active;

	if (thread->state == 0) {
		return FALSE;
	}

	for (active = __OSActiveThreadQueue.head; active; active = active->linkActive.next) {
		if (thread == active) {
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * @TODO: Documentation
 */
s32 OSDisableScheduler(void)
{
	BOOL enabled;
	s32 count;

	enabled = OSDisableInterrupts();
	count   = Reschedule++;
	OSRestoreInterrupts(enabled);
	return count;
}

/**
 * @TODO: Documentation
 */
s32 OSEnableScheduler(void)
{
	BOOL enabled;
	s32 count;

	enabled = OSDisableInterrupts();
	count   = Reschedule--;
	OSRestoreInterrupts(enabled);
	return count;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00006C
 */
static void SetRun(OSThread* thread)
{
	thread->queue = &RunQueue[thread->priority];
	AddTail(thread->queue, thread, link);
	RunQueueBits |= 1u << (OS_PRIORITY_MAX - thread->priority);
	RunQueueHint = TRUE;
}

#pragma dont_inline on

/**
 * @TODO: Documentation
 */
static void UnsetRun(OSThread* thread)
{
	OSThreadQueue* queue;
	queue = thread->queue;
	RemoveItem(queue, thread, link);
	if (queue->head == 0)
		RunQueueBits &= ~(1u << (OS_PRIORITY_MAX - thread->priority));
	thread->queue = NULL;
}

#pragma dont_inline reset

/**
 * @TODO: Documentation
 */
OSPriority __OSGetEffectivePriority(OSThread* thread)
{
	OSPriority priority;
	OSMutex* mutex;
	OSThread* blocked;

	priority = thread->base;
	for (mutex = thread->queueMutex.head; mutex; mutex = mutex->link.next) {
		blocked = mutex->queue.head;
		if (blocked && blocked->priority < priority) {
			priority = blocked->priority;
		}
	}
	return priority;
}

/**
 * @TODO: Documentation
 */
static OSThread* SetEffectivePriority(OSThread* thread, OSPriority priority)
{
	switch (thread->state) {
	case OS_THREAD_STATE_READY:
	{
		UnsetRun(thread);
		thread->priority = priority;
		SetRun(thread);
		break;
	}
	case OS_THREAD_STATE_WAITING:
	{
		RemoveItem(thread->queue, thread, link);
		thread->priority = priority;
		AddPrio(thread->queue, thread, link);
		if (thread->mutex) {
			return thread->mutex->thread;
		}
		break;
	}
	case OS_THREAD_STATE_RUNNING:
	{
		RunQueueHint     = TRUE;
		thread->priority = priority;
		break;
	}
	}
	return NULL;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000078 (Matching by size)
 */
static void UpdatePriority(OSThread* thread)
{
	OSPriority priority;

	do {
		if (thread->suspend > 0) {
			break;
		}
		priority = __OSGetEffectivePriority(thread);
		if (thread->priority == priority) {
			break;
		}
		thread = SetEffectivePriority(thread, priority);
	} while (thread);
}

/**
 * @TODO: Documentation
 */
void __OSPromoteThread(OSThread* thread, OSPriority priority)
{
	do {
		if (thread->suspend > 0) {
			break;
		}
		if (thread->priority <= priority) {
			break;
		}

		thread = SetEffectivePriority(thread, priority);
	} while (thread);
}

/**
 * @TODO: Documentation
 */
static OSThread* SelectThread(BOOL yield)
{
	OSContext* currentContext;
	OSThread* currentThread;
	OSThread* nextThread;
	OSPriority priority;
	OSThreadQueue* queue;

	if (0 < Reschedule) {
		return 0;
	}

	currentContext = OSGetCurrentContext();
	currentThread  = OSGetCurrentThread();
	if (currentContext != &currentThread->context) {
		return 0;
	}

	if (currentThread) {
		if (currentThread->state == OS_THREAD_STATE_RUNNING) {
			if (!yield) {
				#ifndef LIBPORPOISE_PORT
				//TODO
				priority = __mwerks_cntlzw(RunQueueBits);
				#endif
				if (currentThread->priority <= priority) {
					return 0;
				}
			}
			currentThread->state = OS_THREAD_STATE_READY;
			SetRun(currentThread);
		}

		if (!(currentThread->context.state & OS_CONTEXT_STATE_EXC) && OSSaveContext(&currentThread->context)) {
			return 0;
		}
	}

	__OSCurrentThread = NULL;
	if (RunQueueBits == 0) {
		OSSetCurrentContext(&IdleContext);
		do {
			OSEnableInterrupts();
			while (RunQueueBits == 0)
				;
			OSDisableInterrupts();
		} while (RunQueueBits == 0);

		OSClearContext(&IdleContext);
	}

	RunQueueHint = FALSE;

	#ifndef LIBPORPOISE_PORT
	priority = __mwerks_cntlzw(RunQueueBits);
	#endif
	queue    = &RunQueue[priority];
	RemoveHead(queue, nextThread, link);
	if (queue->head == 0) {
		RunQueueBits &= ~(1u << (OS_PRIORITY_MAX - priority));
	}
	nextThread->queue = NULL;
	nextThread->state = OS_THREAD_STATE_RUNNING;
	__OSSwitchThread(nextThread);
	return nextThread;
}

/**
 * @TODO: Documentation
 */
void __OSReschedule(void)
{
	if (!RunQueueHint) {
		return;
	}

	SelectThread(FALSE);
}

/**
 * @TODO: Documentation
 */
void OSYieldThread(void)
{
	BOOL enabled;

#ifdef LIBPORPOISE_PORT
	OSCheckAlarmQueue();
#endif
	enabled = OSDisableInterrupts();
	SelectThread(TRUE);
	OSRestoreInterrupts(enabled);
}

#ifdef LIBPORPOISE_PORT
BOOL OSCreateThreadDebug(OSThread* thread, OSThreadStartFunction func, void* param, void* stack, u32 stackSize, OSPriority priority, u16 attr, const char * name, const char * file, int lineNo) {
	thread->name = name;
	thread->fileName = file;
	thread->lineNo = lineNo;
	return OSCreateThreadReal(thread, func, param, stack, stackSize, priority, attr);
}

void __OSHostThreadWillBlock(OSThreadQueue* threadQueue)
{
	OSThread* currentThread = HostCurrentThread;

	if (currentThread != NULL) {
		currentThread->state = OS_THREAD_STATE_WAITING;
		currentThread->queue = threadQueue;
		AddPrio(threadQueue, currentThread, link);
	}
	if (__OSHostIsAlarmThread()) {
		HostAlarmWaitQueue = threadQueue;
	}
	__OSHostInterruptWillWait();
}

void __OSHostThreadDidWake(void)
{
	OSThread* currentThread;

	__OSHostInterruptDidWait();
	if (__OSHostIsAlarmThread()) {
		HostAlarmWaitQueue = NULL;
	}
	currentThread = HostCurrentThread;
	if (currentThread != NULL) {
		if (currentThread->queue != NULL) {
			RemoveItem(currentThread->queue, currentThread, link);
		}
		currentThread->queue = NULL;
		if (currentThread->state != OS_THREAD_STATE_MORIBUND &&
		    currentThread->state != OS_THREAD_STATE_NULL) {
			currentThread->state = OS_THREAD_STATE_RUNNING;
		}
	}
	OSCheckAlarmQueue();
}

void __OSHostThreadWillExit(void)
{
	OSThread* thread;
	SDL_Thread* detachedThread = NULL;
	BOOL enabled;
	OSHostThreadExitCallback callbacks[
		OS_HOST_THREAD_EXIT_CALLBACK_CAPACITY];
	int callbackCount = 0;
	int i;

	/*
	 * Notify host subsystems while this thread still owns its native resources.
	 * Snapshot under the scheduler lock, then invoke without it: observers may
	 * need to wake OS queues and must not create scheduler -> queue inversions.
	 */
	if (HostCurrentThread != NULL && !HostThreadExitNotified) {
		HostThreadExitNotified = TRUE;
		enabled = OSDisableInterrupts();
		for (i = 0;
		     i < OS_HOST_THREAD_EXIT_CALLBACK_CAPACITY;
		     ++i) {
			if (HostThreadExitCallbacks[i] != NULL) {
				callbacks[callbackCount++] =
					HostThreadExitCallbacks[i];
			}
		}
		OSRestoreInterrupts(enabled);
		for (i = 0; i < callbackCount; ++i) {
			callbacks[i](SDL_ThreadID());
		}
	}

	enabled = OSDisableInterrupts();
	thread = HostCurrentThread;
	if (thread == NULL ||
	    thread->state == OS_THREAD_STATE_MORIBUND ||
	    thread->state == OS_THREAD_STATE_NULL) {
		OSRestoreInterrupts(enabled);
		return;
	}

	if (thread->queue != NULL) {
		RemoveItem(thread->queue, thread, link);
		thread->queue = NULL;
	}
	OSClearContext(&thread->context);
	__OSUnlockAllMutex(thread);

	if (thread->attr & OS_THREAD_ATTR_DETACH) {
		if (__OSIsThreadActive(thread)) {
			RemoveItem(&__OSActiveThreadQueue, thread, linkActive);
		}
		thread->state = OS_THREAD_STATE_NULL;
		detachedThread = thread->sdlThread;
		thread->sdlThread = NULL;
	} else {
		thread->state = OS_THREAD_STATE_MORIBUND;
	}

	OSWakeupThread(&thread->queueJoin);
	OSRestoreInterrupts(enabled);

	if (detachedThread != NULL) {
		SDL_DetachThread(detachedThread);
	}
}

BOOL __OSHostRegisterThreadExitCallback(
	OSHostThreadExitCallback callback)
{
	BOOL enabled;
	BOOL registered = FALSE;
	int emptyIndex = -1;
	int i;

	if (callback == NULL) {
		return FALSE;
	}

	enabled = OSDisableInterrupts();
	for (i = 0; i < OS_HOST_THREAD_EXIT_CALLBACK_CAPACITY; ++i) {
		if (HostThreadExitCallbacks[i] == callback) {
			registered = TRUE;
			break;
		}
		if (emptyIndex < 0 && HostThreadExitCallbacks[i] == NULL) {
			emptyIndex = i;
		}
	}
	if (!registered && emptyIndex >= 0) {
		HostThreadExitCallbacks[emptyIndex] = callback;
		registered = TRUE;
	}
	OSRestoreInterrupts(enabled);
	return registered;
}

int __OSHostWaitForCondition(
	OSThreadQueue* threadQueue,
	SDL_mutex* mutex)
{
	u32 timeout;
	int result;

	/*
	 * Give a due alarm its deterministic service point before committing to
	 * sleep. Return to the caller so it can recheck its queue predicate; an
	 * alarm handler may have completed the very wait we were about to enter.
	 */
	HostBlockingWaitSafePoint = TRUE;
	if (OSCheckAlarmQueue()) {
		HostBlockingWaitSafePoint = FALSE;
		return SDL_MUTEX_TIMEDOUT;
	}
	timeout = __OSHostGetAlarmTimeoutMilliseconds();
	__OSHostThreadWillBlock(threadQueue);
	if (timeout == SDL_MUTEX_MAXWAIT) {
		result = SDL_CondWait(threadQueue->hostCondition, mutex);
	} else {
		result = SDL_CondWaitTimeout(
		    threadQueue->hostCondition,
		    mutex,
		    timeout);
	}
	SDL_UnlockMutex(mutex);
	__OSHostThreadDidWake();
	SDL_LockMutex(mutex);
	HostBlockingWaitSafePoint = FALSE;
	return result;
}

BOOL __OSHostIsBlockingWaitSafePoint(void)
{
	return HostBlockingWaitSafePoint;
}

void __OSHostWakeAlarmThread(void)
{
	OSThreadQueue* threadQueue = HostAlarmWaitQueue;

	if (threadQueue == NULL ||
	    threadQueue->hostMutex == NULL ||
	    threadQueue->hostCondition == NULL) {
		return;
	}
	SDL_LockMutex(threadQueue->hostMutex);
	SDL_CondBroadcast(threadQueue->hostCondition);
	SDL_UnlockMutex(threadQueue->hostMutex);
}

void __OSHostClearAlarmWaitQueue(void)
{
	BOOL enabled = OSDisableInterrupts();

	HostAlarmWaitQueue = NULL;
	OSRestoreInterrupts(enabled);
}

// Wrapper function to run OSThreads in SDL Threads
static int OSRunThread(void * threadPtr) {
	OSThread * thread = (OSThread*)threadPtr;
	void* result;
	BOOL enabled;

	HostCurrentThread = thread;
	HostThreadExitNotified = FALSE;
	enabled = OSDisableInterrupts();
	thread->state = OS_THREAD_STATE_RUNNING;
	OSRestoreInterrupts(enabled);
	result = thread->func(thread->param);
	enabled = OSDisableInterrupts();
	if (thread->state != OS_THREAD_STATE_MORIBUND &&
	    thread->state != OS_THREAD_STATE_NULL) {
		thread->val = result;
	}
	OSRestoreInterrupts(enabled);
	__OSHostThreadWillExit();
	HostCurrentThread = NULL;
	return 0;
}
#endif

/**
 * @TODO: Documentation
 */
#ifdef LIBPORPOISE_PORT
BOOL OSCreateThreadReal(OSThread* thread, OSThreadStartFunction func, void* param, void* stack, u32 stackSize, OSPriority priority, u16 attr)
#else
BOOL OSCreateThread(OSThread* thread, OSThreadStartFunction func, void* param, void* stack, u32 stackSize, OSPriority priority, u16 attr)
#endif
{
	BOOL enable;
	u32 stackThing;
	int i;

	if (priority < OS_PRIORITY_MIN || priority > OS_PRIORITY_MAX) {
		return FALSE;
	}

	stackThing       = ((u32)stack & 0xFFFFFFF8); // ??
	thread->state    = OS_THREAD_STATE_READY;
	thread->attr     = attr & OS_THREAD_ATTR_DETACH;
	thread->base     = priority;
	thread->priority = priority;
	thread->suspend  = 1;
	thread->val      = (void*)-1;
	thread->queue    = NULL;
	thread->mutex    = NULL;
	OSInitThreadQueue(&thread->queueJoin);
	InitMutexQueue(&thread->queueMutex);
	#ifndef LIBPORPOISE_PORT
	*(u32*)(stackThing - 8) = 0;
	*(u32*)(stackThing - 4) = 0;
	#endif

	OSInitContext(&thread->context, (u32)func, (u32)(stackThing - 8));

	#ifndef LIBPORPOISE_PORT
	thread->context.lr     = (u32)&OSExitThread;
	thread->context.gpr[3] = (u32)param;
	thread->stackBase      = stack;
	thread->stackEnd       = (u32*)((u32)stack - stackSize);
	*(thread->stackEnd)    = OS_THREAD_STACK_MAGIC;
	#else
	thread->stackBase      = stack;
	thread->stackEnd       = &thread->hostStackMagic;
	thread->hostStackMagic = OS_THREAD_STACK_MAGIC;
	#endif

	enable = OSDisableInterrupts();

	AddTail(&__OSActiveThreadQueue, thread, linkActive);
	OSRestoreInterrupts(enable);
#ifdef LIBPORPOISE_PORT
	thread->func = func;
	thread->param = param;
	thread->sdlThread = NULL;
#endif
	return TRUE;
}

/**
 * @TODO: Documentation
 */
void OSExitThread(void* val)
{
#ifdef LIBPORPOISE_PORT
	OSThread* thread;
	BOOL enable;

	enable = OSDisableInterrupts();
	thread = HostCurrentThread;
	if (thread != NULL &&
	    thread->state != OS_THREAD_STATE_MORIBUND &&
	    thread->state != OS_THREAD_STATE_NULL) {
		thread->val = val;
	}
	OSRestoreInterrupts(enable);
	__OSHostThreadWillExit();
#else
	OSThread* thread;
	BOOL enable;

	enable = OSDisableInterrupts();
	thread = __OSCurrentThread;
	OSClearContext(&thread->context);

	if (thread->attr & OS_THREAD_ATTR_DETACH) {
		RemoveItem(&__OSActiveThreadQueue, thread, linkActive);
		thread->state = OS_THREAD_STATE_NULL;

	} else {
		thread->state = OS_THREAD_STATE_MORIBUND;
		thread->val   = val;
	}

	__OSUnlockAllMutex(thread);
	OSWakeupThread(&thread->queueJoin);
	RunQueueHint = TRUE;
	if (RunQueueHint != FALSE) {
		SelectThread(FALSE);
	}

	OSRestoreInterrupts(enable);
#endif
}

/**
 * @TODO: Documentation
 */
void OSCancelThread(OSThread* thread)
{
	BOOL enabled;

	enabled = OSDisableInterrupts();

	switch (thread->state) {
	case OS_THREAD_STATE_READY:
	{
		if (!(0 < thread->suspend)) {
			UnsetRun(thread);
		}
		break;
	}
	case OS_THREAD_STATE_RUNNING:
	{
		RunQueueHint = TRUE;
		break;
	}
	case OS_THREAD_STATE_WAITING:
	{
		RemoveItem(thread->queue, thread, link);
		thread->queue = NULL;
		if (!(0 < thread->suspend) && thread->mutex) {
			UpdatePriority(thread->mutex->thread);
		}
		break;
	}
	default:
	{
		OSRestoreInterrupts(enabled);
		return;
	}
	}

	OSClearContext(&thread->context);
	if (thread->attr & OS_THREAD_ATTR_DETACH) {
		RemoveItem(&__OSActiveThreadQueue, thread, linkActive);
		thread->state = OS_THREAD_STATE_NULL;
	} else {
		thread->state = OS_THREAD_STATE_MORIBUND;
	}

	__OSUnlockAllMutex(thread);

	OSWakeupThread(&thread->queueJoin);

	__OSReschedule();
	OSRestoreInterrupts(enabled);

	return;
}

/**
 * @TODO: Documentation
 */
BOOL OSJoinThread(OSThread* thread, void** val)
{
#ifdef LIBPORPOISE_PORT
	BOOL enabled;

	if ((thread->attr & OS_THREAD_ATTR_DETACH) != 0 ||
	    thread->sdlThread == NULL) {
		return FALSE;
	}

	enabled = OSDisableInterrupts();
	SDL_LockMutex(thread->queueJoin.hostMutex);
	while (thread->state != OS_THREAD_STATE_MORIBUND) {
		__OSHostWaitForCondition(
		    &thread->queueJoin,
		    thread->queueJoin.hostMutex);
	}
	SDL_UnlockMutex(thread->queueJoin.hostMutex);
	OSRestoreInterrupts(enabled);
	SDL_WaitThread(thread->sdlThread, NULL);

	enabled = OSDisableInterrupts();
	thread->sdlThread = NULL;
	if (val != NULL) {
		*val = thread->val;
	}
	if (__OSIsThreadActive(thread)) {
		RemoveItem(&__OSActiveThreadQueue, thread, linkActive);
	}
	thread->state = OS_THREAD_STATE_NULL;
	OSRestoreInterrupts(enabled);
	return TRUE;
#else
	BOOL enabled = OSDisableInterrupts();

	if (!(thread->attr & 1) && (thread->state != 8) && (thread->queueJoin.head == NULL)) {
		OSSleepThread(&thread->queueJoin);
		if (__OSIsThreadActive(thread) == 0) {
			OSRestoreInterrupts(enabled);
			return 0;
		}
	}

	if (thread->state == 8) {
		if (val) {
			*(s32*)val = (s32)thread->val;
		}
		RemoveItem(&__OSActiveThreadQueue, thread, linkActive);
		thread->state = 0;
		OSRestoreInterrupts(enabled);
		return 1;
	}
	OSRestoreInterrupts(enabled);
	return 0;
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 0000A0
 */
void OSDetachThread(OSThread* thread)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 */
s32 OSResumeThread(OSThread* thread)
{
#ifdef LIBPORPOISE_PORT
	BOOL enabled;
	s32 suspendCount;

	enabled = OSDisableInterrupts();
	suspendCount = thread->suspend;
	if (thread->suspend > 0) {
		thread->suspend--;
	}
	if (thread->func != NULL && thread->sdlThread == NULL) {
		thread->sdlThread = SDL_CreateThread(OSRunThread, thread->name, (void*)thread);
	} else {
		if (thread->func == NULL) {
			thread->state = OS_THREAD_STATE_MORIBUND;
		}
	}
	OSRestoreInterrupts(enabled);
	return suspendCount;
#else
	BOOL enabled;
	s32 suspendCount;

	enabled      = OSDisableInterrupts();
	suspendCount = thread->suspend--;
	if (thread->suspend < 0) {
		thread->suspend = 0;
	} else if (thread->suspend == 0) {
		switch (thread->state) {
		case OS_THREAD_STATE_READY:
		{
			thread->priority = __OSGetEffectivePriority(thread);
			SetRun(thread);
			break;
		}
		case OS_THREAD_STATE_WAITING:
		{
			RemoveItem(thread->queue, thread, link);
			thread->priority = __OSGetEffectivePriority(thread);
			AddPrio(thread->queue, thread, link);
			if (thread->mutex) {
				UpdatePriority(thread->mutex->thread);
			}
			break;
		}
		}
		__OSReschedule();
	}
	OSRestoreInterrupts(enabled);
	return suspendCount;
#endif
}

/**
 * @TODO: Documentation
 */
s32 OSSuspendThread(OSThread* thread)
{
	BOOL enabled;
	s32 suspendCount;

	enabled      = OSDisableInterrupts();
	suspendCount = thread->suspend++;
	if (suspendCount == 0) {
		switch (thread->state) {
		case OS_THREAD_STATE_RUNNING:
		{
			RunQueueHint  = TRUE;
			thread->state = OS_THREAD_STATE_READY;
			break;
		}
		case OS_THREAD_STATE_READY:
		{
			UnsetRun(thread);
			break;
		}
		case OS_THREAD_STATE_WAITING:
		{
			RemoveItem(thread->queue, thread, link);
			thread->priority = 32;
			AddTail(thread->queue, thread, link);
			if (thread->mutex) {
				UpdatePriority(thread->mutex->thread);
			}
			break;
		}
		}

		__OSReschedule();
	}
	OSRestoreInterrupts(enabled);
	return suspendCount;
}

/**
 * @TODO: Documentation
 */
void OSSleepThread(OSThreadQueue* threadQueue)
{
#ifdef LIBPORPOISE_PORT
	BOOL enabled;
	u64 wakeGeneration;

	if (threadQueue == NULL) {
		return;
	}
	if (threadQueue->hostMutex == NULL ||
	    threadQueue->hostCondition == NULL) {
		OSInitThreadQueue(threadQueue);
	}

	enabled = OSDisableInterrupts();
	SDL_LockMutex(threadQueue->hostMutex);
	wakeGeneration = threadQueue->hostWakeGeneration;
	while (wakeGeneration == threadQueue->hostWakeGeneration) {
		__OSHostWaitForCondition(threadQueue, threadQueue->hostMutex);
	}
	SDL_UnlockMutex(threadQueue->hostMutex);
	OSRestoreInterrupts(enabled);
#else
	BOOL enabled;
	OSThread* currentThread;

	enabled       = OSDisableInterrupts();
	currentThread = OSGetCurrentThread();

	currentThread->state = OS_THREAD_STATE_WAITING;
	currentThread->queue = threadQueue;
	AddPrio(threadQueue, currentThread, link);
	RunQueueHint = TRUE;
	__OSReschedule();
	OSRestoreInterrupts(enabled);
#endif
}

/**
 * @TODO: Documentation
 */
void OSWakeupThread(OSThreadQueue* threadQueue)
{
#ifdef LIBPORPOISE_PORT
	BOOL enabled;

	if (threadQueue == NULL ||
	    threadQueue->hostMutex == NULL ||
	    threadQueue->hostCondition == NULL) {
		return;
	}
	enabled = OSDisableInterrupts();
	SDL_LockMutex(threadQueue->hostMutex);
	threadQueue->hostWakeGeneration++;
	SDL_CondBroadcast(threadQueue->hostCondition);
	SDL_UnlockMutex(threadQueue->hostMutex);
	OSRestoreInterrupts(enabled);
#else
	BOOL enabled;
	OSThread* thread;

	enabled = OSDisableInterrupts();
	while (threadQueue->head) {
		RemoveHead(threadQueue, thread, link);
		thread->state = OS_THREAD_STATE_READY;
		if (!(0 < thread->suspend)) {
			SetRun(thread);
		}
	}
	__OSReschedule();
	OSRestoreInterrupts(enabled);
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 0000C0
 */
BOOL OSSetThreadPriority(OSThread*, s32)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 */
OSPriority OSGetThreadPriority(OSThread* thread)
{
	return thread->base;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000090
 */
OSThread* OSSetIdleFunction(OSIdleFunction idleFunc, void* param, void* stack, u32 stackSize)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00001C
 */
OSThread* OSGetIdleFunction(void)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 */
static int CheckThreadQueue(OSThreadQueue* queue)
{
	OSThread* thread;

	if ((queue->head != NULL) && (queue->head->link.prev != NULL)) {
		return 0;
	}
	if ((queue->tail != NULL) && (queue->tail->link.next != NULL)) {
		return 0;
	}
	thread = queue->head;
	while (thread) {
		if ((thread->link.next != NULL) && (thread != thread->link.next->link.prev)) {
			return 0;
		}
		if ((thread->link.prev != NULL) && (thread != thread->link.prev->link.next)) {
			return 0;
		}
		thread = thread->link.next;
	}
	return 1;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00002C (Matching by size)
 */
static BOOL IsMember(OSThreadQueue* queue, OSThread* thread)
{
	struct OSThread* member = queue->head;

	while (member) {
		if (thread == member) {
			return 1;
		}
		member = member->link.next;
	}
	return 0;
}

// custom macro for OSCheckActiveThreads?
#define ASSERTREPORT(line, cond)                                          \
	if (!(cond)) {                                                        \
		OSReport("OSCheckActiveThreads: Failed " #cond " in %d\n", line); \
		OSErrorLine(line, "");                                            \
	}

#define IsSuspended(suspend) (suspend > 0)

/**
 * @TODO: Documentation
 */
s32 OSCheckActiveThreads(void)
{
	OSThread* thread;
	s32 prio;
	s32 cThread;
	int enabled;

	cThread = 0;
	enabled = OSDisableInterrupts();

	for (prio = 0; prio <= 0x1F; prio++) {
		if (RunQueueBits & (1 << (0x1F - prio))) {
			ASSERTREPORT(0x566, RunQueue[prio].head != NULL && RunQueue[prio].tail != NULL);
		} else {
			ASSERTREPORT(0x56B, RunQueue[prio].head == NULL && RunQueue[prio].tail == NULL);
		}
		ASSERTREPORT(0x56D, CheckThreadQueue(&RunQueue[prio]));
	}

	ASSERTREPORT(0x572, __OSActiveThreadQueue.head == NULL || __OSActiveThreadQueue.head->linkActive.prev == NULL);
	ASSERTREPORT(0x574, __OSActiveThreadQueue.tail == NULL || __OSActiveThreadQueue.tail->linkActive.next == NULL);

	thread = __OSActiveThreadQueue.head;
	while (thread) {
		cThread++;
		ASSERTREPORT(0x57C, thread->linkActive.next == NULL || thread == thread->linkActive.next->linkActive.prev);
		ASSERTREPORT(0x57E, thread->linkActive.prev == NULL || thread == thread->linkActive.prev->linkActive.next);
		ASSERTREPORT(0x581, *(thread->stackEnd) == OS_THREAD_STACK_MAGIC);

		// need to not have spaces around the plus in the line below
		// clang-format off
		ASSERTREPORT(0x584, OS_PRIORITY_MIN <= thread->priority && thread->priority <= OS_PRIORITY_MAX+1);
		// clang-format on

		ASSERTREPORT(0x585, 0 <= thread->suspend);
		ASSERTREPORT(0x586, CheckThreadQueue(&thread->queueJoin));

		switch (thread->state) {
		case 1:
		{
			if (thread->suspend <= 0) {
				ASSERTREPORT(0x58C, thread->queue == &RunQueue[thread->priority]);
				ASSERTREPORT(0x58D, IsMember(&RunQueue[thread->priority], thread));
				ASSERTREPORT(0x58E, thread->priority == __OSGetEffectivePriority(thread));
			}
			break;
		}
		case 2:
		{
			ASSERTREPORT(0x592, !IsSuspended(thread->suspend));
			ASSERTREPORT(0x593, thread->queue == NULL);
			ASSERTREPORT(0x594, thread->priority == __OSGetEffectivePriority(thread));
			break;
		}
		case 4:
		{
			ASSERTREPORT(0x597, thread->queue != NULL);
			ASSERTREPORT(0x598, CheckThreadQueue(thread->queue));
			ASSERTREPORT(0x599, IsMember(thread->queue, thread));
			if (thread->suspend <= 0) {
				ASSERTREPORT(0x59C, thread->priority == __OSGetEffectivePriority(thread));
			} else {
				ASSERTREPORT(0x5A0, thread->priority == 32);
			}
			ASSERTREPORT(0x5A2, !__OSCheckDeadLock(thread));
			break;
		}
		case 8:
		{
			ASSERTREPORT(0x5A6, thread->queueMutex.head == NULL && thread->queueMutex.tail == NULL);
			break;
		}
		default:
		{
			OSReport("OSCheckActiveThreads: Failed. unkown thread state (%d) of thread %p\n", thread->state, thread);
			OSErrorLine(0x5AC, "");
		}
		}
		ASSERTREPORT(0x5B1, __OSCheckMutexes(thread));
		thread = thread->linkActive.next;
	}
	OSRestoreInterrupts(enabled);
	return cThread;
}
