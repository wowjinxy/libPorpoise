#include <dolphin/os.h>
#include <stddef.h>

#ifdef LIBPORPOISE_PORT
static BOOL LockHostMutex(OSMutex* mutex)
{
	int result;

	for (;;) {
		result = SDL_TryLockMutex(mutex->sdlMutex);
		if (result == 0) {
			return TRUE;
		}
		if (result != SDL_MUTEX_TIMEDOUT) {
			return FALSE;
		}

		/*
		 * Do not retain the single-core scheduler lock while waiting for an
		 * object mutex. Drop the object immediately after it becomes
		 * available, reacquire the scheduler, then retry in the canonical
		 * scheduler-before-object order.
		 */
		__OSHostThreadWillWait(&mutex->queue);
		result = SDL_LockMutex(mutex->sdlMutex);
		if (result == 0) {
			SDL_UnlockMutex(mutex->sdlMutex);
		}
		__OSHostThreadDidWait();
		if (result != 0) {
			return FALSE;
		}
	}
}
#endif

/**
 * @TODO: Documentation
 */
void OSInitMutex(OSMutex* mutex)
{
	OSInitThreadQueue(&mutex->queue);
	mutex->thread = NULL;
	mutex->count  = 0;
	#ifdef LIBPORPOISE_PORT
	mutex->sdlMutex = SDL_CreateMutex();
	mutex->hostOwner = 0;
	#endif
}

/**
 * @TODO: Documentation
 */
void OSLockMutex(OSMutex* mutex)
{
	#ifdef LIBPORPOISE_PORT
	BOOL enabled = OSDisableInterrupts();
	SDL_threadID currentThread = SDL_ThreadID();

	if (LockHostMutex(mutex)) {
		if (mutex->hostOwner == currentThread) {
			mutex->count++;
		} else {
			mutex->hostOwner = currentThread;
			mutex->thread = OSGetCurrentThread();
			mutex->count = 1;
		}
	}
	OSRestoreInterrupts(enabled);
	#else
	BOOL enabled            = OSDisableInterrupts();
	OSThread* currentThread = OSGetCurrentThread();
	OSThread* ownerThread;

	while (TRUE) {
		ownerThread = mutex->thread;
		if (ownerThread == 0) {
			mutex->thread = currentThread;
			mutex->count++;
			AddTailMutex(&currentThread->queueMutex, mutex, link);
			break;
		} else if (ownerThread == currentThread) {
			mutex->count++;
			break;
		} else {
			currentThread->mutex = mutex;
			__OSPromoteThread(mutex->thread, currentThread->priority);
			OSSleepThread(&mutex->queue);
			currentThread->mutex = NULL;
		}
	}
	OSRestoreInterrupts(enabled);
	#endif
}

/**
 * @TODO: Documentation
 */
void OSUnlockMutex(OSMutex* mutex)
{
	#ifdef LIBPORPOISE_PORT
	BOOL enabled = OSDisableInterrupts();

	if (mutex->hostOwner == SDL_ThreadID() && mutex->count > 0) {
		mutex->count--;
		if (mutex->count == 0) {
			mutex->hostOwner = 0;
			mutex->thread = NULL;
		}
		SDL_UnlockMutex(mutex->sdlMutex);
	}
	OSRestoreInterrupts(enabled);
	#else
	BOOL enabled            = OSDisableInterrupts();
	OSThread* currentThread = OSGetCurrentThread();

	if (mutex->thread == currentThread && --mutex->count == 0) {
		RemoveItemMutex(&currentThread->queueMutex, mutex, link);
		mutex->thread = NULL;
		if (currentThread->priority < currentThread->base) {
			currentThread->priority = __OSGetEffectivePriority(currentThread);
		}

		OSWakeupThread(&mutex->queue);
	}
	OSRestoreInterrupts(enabled);
	#endif
}

/**
 * @TODO: Documentation
 */
void __OSUnlockAllMutex(OSThread* thread)
{
	OSMutex* mutex;

	while (thread->queueMutex.head) {
		RemoveHeadMutex(&thread->queueMutex, mutex, link);
		mutex->count  = 0;
		mutex->thread = NULL;
		OSWakeupThread(&mutex->queue);
	}
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 0000BC
 */
BOOL OSTryLockMutex(OSMutex* mutex)
{
	#ifdef LIBPORPOISE_PORT
	BOOL enabled = OSDisableInterrupts();
	SDL_threadID currentThread = SDL_ThreadID();
	BOOL locked = SDL_TryLockMutex(mutex->sdlMutex) == 0;

	if (locked) {
		if (mutex->hostOwner == currentThread) {
			mutex->count++;
		} else {
			mutex->hostOwner = currentThread;
			mutex->thread = OSGetCurrentThread();
			mutex->count = 1;
		}
	}
	OSRestoreInterrupts(enabled);
	return locked;
	#else
	TRAP_UNIMPLEMENTED;
	#endif
}

/**
 * @TODO: Documentation
 */
void OSInitCond(OSCond* cond)
{
	#ifdef LIBPORPOISE_PORT
	OSInitThreadQueue(&cond->queue);
	cond->sdlSemaphore = SDL_CreateSemaphore(0);
	#else
	OSInitThreadQueue(&cond->queue);
	#endif
}

/**
 * @TODO: Documentation
 */
void OSWaitCond(OSCond* cond, OSMutex* mutex)
{
	#ifdef LIBPORPOISE_PORT
	BOOL enabled = OSDisableInterrupts();
	s32 count;
	s32 i;

	if (mutex->hostOwner != SDL_ThreadID() || mutex->count <= 0) {
		OSRestoreInterrupts(enabled);
		return;
	}

	count = mutex->count;
	mutex->count = 0;
	mutex->hostOwner = 0;
	mutex->thread = NULL;
	for (i = 0; i < count; ++i) {
		SDL_UnlockMutex(mutex->sdlMutex);
	}

	__OSHostThreadWillWait(&cond->queue);
	SDL_SemWait(cond->sdlSemaphore);
	__OSHostThreadDidWait();

	for (i = 0; i < count; ++i) {
		OSLockMutex(mutex);
	}
	OSRestoreInterrupts(enabled);
	#else
	BOOL enabled            = OSDisableInterrupts();
	OSThread* currentThread = OSGetCurrentThread();
	s32 count;

	if (mutex->thread == currentThread) {
		count        = mutex->count;
		mutex->count = 0;
		RemoveItemMutex(&currentThread->queueMutex, mutex, link);
		mutex->thread = NULL;

		if (currentThread->priority < currentThread->base) {
			currentThread->priority = __OSGetEffectivePriority(currentThread);
		}

		OSDisableScheduler();
		OSWakeupThread(&mutex->queue);
		OSEnableScheduler();
		OSSleepThread(&cond->queue);
		OSLockMutex(mutex);
		mutex->count = count;
	}

	OSRestoreInterrupts(enabled);
	#endif
}

/**
 * @TODO: Documentation
 */
void OSSignalCond(OSCond* cond)
{
	#ifdef LIBPORPOISE_PORT
	BOOL enabled = OSDisableInterrupts();
	SDL_SemPost(cond->sdlSemaphore);
	OSRestoreInterrupts(enabled);
	#else
	OSWakeupThread(&cond->queue);
	#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00002C
 */
static void IsMember(void)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 */
BOOL __OSCheckMutex(OSMutex* mutex)
{
	OSThread* thread;
	OSThreadQueue* queue;
	s32 priority;

	priority = 0;
	queue    = &mutex->queue;

	if (queue->head != NULL && queue->head->link.prev != NULL) {
		return 0;
	}
	if (queue->tail != NULL && queue->tail->link.next != NULL) {
		return 0;
	}
	thread = queue->head;
	while (thread) {
		if (thread->link.next != NULL && (thread != thread->link.next->link.prev)) {
			return 0;
		}
		if (thread->link.prev != NULL && (thread != thread->link.prev->link.next)) {
			return 0;
		}
		if (thread->state != 4) {
			return 0;
		}
		if (thread->priority < priority) {
			return 0;
		}
		priority = thread->priority;
		thread   = thread->link.next;
	}
	if (mutex->thread) {
		if (mutex->count <= 0) {
			return 0;
		}
	} else if (mutex->count != 0) {
		return 0;
	}
	return 1;
}

/**
 * @TODO: Documentation
 */
BOOL __OSCheckDeadLock(OSThread* thread)
{
	OSMutex* mutex = thread->mutex;

	while (mutex && mutex->thread) {
		if (mutex->thread == thread) {
			return 1;
		}
		mutex = mutex->thread->mutex;
	}
	return 0;
}

/**
 * @TODO: Documentation
 */
BOOL __OSCheckMutexes(OSThread* thread)
{
	OSMutex* mutex = thread->queueMutex.head;

	while (mutex) {
		if (mutex->thread != thread) {
			return 0;
		}
		if (__OSCheckMutex(mutex) == 0) {
			return 0;
		}
		mutex = mutex->link.next;
	}
	return 1;
}
