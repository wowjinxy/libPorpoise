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
		 * object mutex. Its thread queue is signaled when the owner releases
		 * the final recursive lock; alarm timeouts may also wake this wait
		 * without completing it.
		 */
		SDL_LockMutex(mutex->queue.hostMutex);
		result = __OSHostWaitForCondition(
		    &mutex->queue,
		    mutex->queue.hostMutex);
		SDL_UnlockMutex(mutex->queue.hostMutex);
		if (result != 0 && result != SDL_MUTEX_TIMEDOUT) {
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
	BOOL released = FALSE;

	if (mutex->hostOwner == SDL_ThreadID() && mutex->count > 0) {
		mutex->count--;
		if (mutex->count == 0) {
			mutex->hostOwner = 0;
			mutex->thread = NULL;
			released = TRUE;
		}
		SDL_UnlockMutex(mutex->sdlMutex);
	}
	if (released) {
		OSWakeupThread(&mutex->queue);
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
	u64 wakeGeneration;

	if (mutex->hostOwner != SDL_ThreadID() || mutex->count <= 0) {
		OSRestoreInterrupts(enabled);
		return;
	}

	count = mutex->count;
	mutex->count = 0;
	mutex->hostOwner = 0;
	mutex->thread = NULL;
	SDL_LockMutex(cond->queue.hostMutex);
	wakeGeneration = cond->queue.hostWakeGeneration;
	for (i = 0; i < count; ++i) {
		SDL_UnlockMutex(mutex->sdlMutex);
	}

	OSWakeupThread(&mutex->queue);
	while (wakeGeneration == cond->queue.hostWakeGeneration) {
		__OSHostWaitForCondition(
		    &cond->queue,
		    cond->queue.hostMutex);
	}
	SDL_UnlockMutex(cond->queue.hostMutex);

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
	SDL_LockMutex(cond->queue.hostMutex);
	cond->queue.hostWakeGeneration++;
	SDL_CondBroadcast(cond->queue.hostCondition);
	SDL_UnlockMutex(cond->queue.hostMutex);
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
