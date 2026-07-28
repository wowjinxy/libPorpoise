#include <dolphin/os.h>
#ifdef LIBPORPOISE_PORT
#include <SDL2/SDL_mutex.h>
#endif

#ifdef LIBPORPOISE_PORT
static BOOL WaitForMessageQueue(
	OSMessageQueue* queue,
	OSThreadQueue* waitQueue)
{
	int result;

	__OSHostThreadWillWait(waitQueue);
	result = SDL_CondWait(waitQueue->hostCondition, queue->hostMutex);
	__OSHostThreadDidWait();
	return result == 0;
}
#endif

/**
 * @TODO: Documentation
 */
void OSInitMessageQueue(OSMessageQueue* queue, OSMessage* msgArray, s32 msgCount)
{
	OSInitThreadQueue(&queue->queueSend);
	OSInitThreadQueue(&queue->queueReceive);
	queue->msgArray   = msgArray;
	queue->msgCount   = msgCount;
	queue->firstIndex = 0;
	queue->usedCount  = 0;
#ifdef LIBPORPOISE_PORT
	queue->hostMutex = SDL_CreateMutex();
#endif
}

/**
 * @TODO: Documentation
 */
BOOL OSSendMessage(OSMessageQueue* queue, OSMessage msg, s32 flags)
{
	int mesgId;
	u32 interrupt;

	interrupt = OSDisableInterrupts();

#ifdef LIBPORPOISE_PORT
	if (queue == NULL || queue->hostMutex == NULL) {
		OSRestoreInterrupts(interrupt);
		return FALSE;
	}
	SDL_LockMutex(queue->hostMutex);
#endif
	while (queue->msgCount <= queue->usedCount) {
		if (!(flags & OS_MSG_PERSISTENT)) {
#ifdef LIBPORPOISE_PORT
			SDL_UnlockMutex(queue->hostMutex);
#endif
			OSRestoreInterrupts(interrupt);
			return FALSE;
		}

#ifdef LIBPORPOISE_PORT
		if (!WaitForMessageQueue(queue, &queue->queueSend)) {
			SDL_UnlockMutex(queue->hostMutex);
			OSRestoreInterrupts(interrupt);
			return FALSE;
		}
#else
		OSSleepThread(&queue->queueSend);
#endif
	}

	mesgId                  = (queue->firstIndex + queue->usedCount) % queue->msgCount;
	queue->msgArray[mesgId] = msg;
	queue->usedCount++;

#ifdef LIBPORPOISE_PORT
	SDL_CondBroadcast(queue->queueReceive.hostCondition);
	SDL_UnlockMutex(queue->hostMutex);
#else
	OSWakeupThread(&queue->queueReceive);
#endif
	OSRestoreInterrupts(interrupt);
	return TRUE;
}

/**
 * @TODO: Documentation
 */
BOOL OSReceiveMessage(OSMessageQueue* queue, OSMessage* buffer, s32 flags)
{
	u32 interrupt;

	interrupt = OSDisableInterrupts();

#ifdef LIBPORPOISE_PORT
	if (queue == NULL || queue->hostMutex == NULL) {
		OSRestoreInterrupts(interrupt);
		return FALSE;
	}
	SDL_LockMutex(queue->hostMutex);
#endif
	while (queue->usedCount == 0) {
		if (!(flags & OS_MSG_PERSISTENT)) {
#ifdef LIBPORPOISE_PORT
			SDL_UnlockMutex(queue->hostMutex);
#endif
			OSRestoreInterrupts(interrupt);
			return FALSE;
		}

#ifdef LIBPORPOISE_PORT
		if (!WaitForMessageQueue(queue, &queue->queueReceive)) {
			SDL_UnlockMutex(queue->hostMutex);
			OSRestoreInterrupts(interrupt);
			return FALSE;
		}
#else
		OSSleepThread(&queue->queueReceive);
#endif
	}

	if (buffer) {
		buffer[0] = queue->msgArray[queue->firstIndex];
	}

	queue->firstIndex = (queue->firstIndex + 1) % queue->msgCount;
	queue->usedCount--;

#ifdef LIBPORPOISE_PORT
	SDL_CondBroadcast(queue->queueSend.hostCondition);
	SDL_UnlockMutex(queue->hostMutex);
#else
	OSWakeupThread(&queue->queueSend);
#endif
	OSRestoreInterrupts(interrupt);
	return TRUE;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 0000D4
 */
BOOL OSJamMessage(OSMessageQueue* queue, OSMessage msg, s32 flags)
{
	u32 interrupt;

	interrupt = OSDisableInterrupts();

#ifdef LIBPORPOISE_PORT
	if (queue == NULL || queue->hostMutex == NULL) {
		OSRestoreInterrupts(interrupt);
		return FALSE;
	}
	SDL_LockMutex(queue->hostMutex);
#endif
	while (queue->msgCount <= queue->usedCount) {
		if (!(flags & OS_MSG_PERSISTENT)) {
#ifdef LIBPORPOISE_PORT
			SDL_UnlockMutex(queue->hostMutex);
#endif
			OSRestoreInterrupts(interrupt);
			return FALSE;
		}

#ifdef LIBPORPOISE_PORT
		if (!WaitForMessageQueue(queue, &queue->queueSend)) {
			SDL_UnlockMutex(queue->hostMutex);
			OSRestoreInterrupts(interrupt);
			return FALSE;
		}
#else
		OSSleepThread(&queue->queueSend);
#endif
	}

	queue->firstIndex =
	    (queue->firstIndex + queue->msgCount - 1) % queue->msgCount;
	queue->msgArray[queue->firstIndex] = msg;
	queue->usedCount++;

#ifdef LIBPORPOISE_PORT
	SDL_CondBroadcast(queue->queueReceive.hostCondition);
	SDL_UnlockMutex(queue->hostMutex);
#else
	OSWakeupThread(&queue->queueReceive);
#endif
	OSRestoreInterrupts(interrupt);
	return TRUE;
}
