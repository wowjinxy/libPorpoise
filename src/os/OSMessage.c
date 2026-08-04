#include <dolphin/os.h>
#ifdef LIBPORPOISE_PORT
#include <SDL2/SDL_mutex.h>
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
	queue->sdlSemaphore = (void*)SDL_CreateSemaphore(0);
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

	while (queue->msgCount <= queue->usedCount) {
		if (!(flags & OS_MSG_PERSISTENT)) {
			OSRestoreInterrupts(interrupt);
			return FALSE;
		}

		OSSleepThread(&queue->queueSend);
	}

	mesgId                  = (queue->firstIndex + queue->usedCount) % queue->msgCount;
	queue->msgArray[mesgId] = msg;
	queue->usedCount++;


	#ifdef LIBPORPOISE_PORT
	SDL_SemPost((SDL_sem*)queue->sdlSemaphore);
	#endif

	OSWakeupThread(&queue->queueReceive);
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

	while (queue->usedCount == 0) {
		#ifdef LIBPORPOISE_PORT
		SDL_SemWait((SDL_sem*)queue->sdlSemaphore);
		#else
		if (!(flags & OS_MSG_PERSISTENT)) {
			OSRestoreInterrupts(interrupt);
			return FALSE;
		}

		OSSleepThread(&queue->queueReceive);
		#endif
	}

	if (buffer) {
		buffer[0] = queue->msgArray[queue->firstIndex];
	}

	queue->firstIndex = (queue->firstIndex + 1) % queue->msgCount;
	queue->usedCount--;

	OSWakeupThread(&queue->queueSend);
	OSRestoreInterrupts(interrupt);
	return TRUE;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 0000D4
 */
BOOL OSJamMessage(OSMessageQueue* queue, OSMessage msg, s32 flags)
{
	TRAP_UNIMPLEMENTED;
}
