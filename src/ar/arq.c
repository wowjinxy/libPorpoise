#include <dolphin/ar.h>
#include <dolphin/os.h>
#ifdef LIBPORPOISE_PORT
#include <dolphin/os/OSHostAddress.h>
#endif
#include <stddef.h>

static ARQRequest* __ARQRequestQueueHi;
static ARQRequest* __ARQRequestTailHi;
static ARQRequest* __ARQRequestQueueLo;
static ARQRequest* __ARQRequestTailLo;
static ARQRequest* __ARQRequestPendingHi;
static ARQRequest* __ARQRequestPendingLo;
static ARQCallback __ARQCallbackHi;
static ARQCallback __ARQCallbackLo;
static u32 __ARQChunkSize;

static volatile BOOL __ARQ_init_flag = FALSE;

#ifdef LIBPORPOISE_PORT
static ARQRequest* __ARQHostPending;
static ARQCallback __ARQHostCallback;
#endif

static void __ARQInvokeCallback(
	ARQCallback callback,
	ARQRequest* request)
{
#ifdef LIBPORPOISE_PORT
	u32 requestToken = __OSHostEncodeAddress(request);
	(*callback)(requestToken);
	__OSHostReleaseAddress(requestToken);
#else
	(*callback)((u32)request);
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000070
 */
void __ARQPopTaskQueueHi()
{
	if (__ARQRequestQueueHi) {
		if (__ARQRequestQueueHi->type == ARQ_TYPE_MRAM_TO_ARAM) {
			ARStartDMA(__ARQRequestQueueHi->type, __ARQRequestQueueHi->source, __ARQRequestQueueHi->dest, __ARQRequestQueueHi->length);
		} else {
			ARStartDMA(__ARQRequestQueueHi->type, __ARQRequestQueueHi->dest, __ARQRequestQueueHi->source, __ARQRequestQueueHi->length);
		}

		__ARQCallbackHi = __ARQRequestQueueHi->callback;

		__ARQRequestPendingHi = __ARQRequestQueueHi;

		__ARQRequestQueueHi = __ARQRequestQueueHi->next;
	}
}

/**
 * @TODO: Documentation
 */
void __ARQServiceQueueLo()
{
	if ((__ARQRequestPendingLo == NULL) && (__ARQRequestQueueLo)) {
		__ARQRequestPendingLo = __ARQRequestQueueLo;
		__ARQRequestQueueLo   = __ARQRequestQueueLo->next;
	}

	if (__ARQRequestPendingLo) {
		if (__ARQRequestPendingLo->length <= __ARQChunkSize) {

			if (__ARQRequestPendingLo->type == ARQ_TYPE_MRAM_TO_ARAM) {
				ARStartDMA(__ARQRequestPendingLo->type, __ARQRequestPendingLo->source, __ARQRequestPendingLo->dest,
				           __ARQRequestPendingLo->length);
			} else {
				ARStartDMA(__ARQRequestPendingLo->type, __ARQRequestPendingLo->dest, __ARQRequestPendingLo->source,
				           __ARQRequestPendingLo->length);
			}

			__ARQCallbackLo = __ARQRequestPendingLo->callback;

		} else if (__ARQRequestPendingLo->type == ARQ_TYPE_MRAM_TO_ARAM) {
			ARStartDMA(__ARQRequestPendingLo->type, __ARQRequestPendingLo->source, __ARQRequestPendingLo->dest, __ARQChunkSize);

		} else {
			ARStartDMA(__ARQRequestPendingLo->type, __ARQRequestPendingLo->dest, __ARQRequestPendingLo->source, __ARQChunkSize);
		}

		__ARQRequestPendingLo->length -= __ARQChunkSize;
		__ARQRequestPendingLo->source += __ARQChunkSize;
		__ARQRequestPendingLo->dest += __ARQChunkSize;
	}
}

/**
 * @TODO: Documentation
 */
void __ARQCallbackHack(void)
{
}

/**
 * @TODO: Documentation
 */
void __ARQInterruptServiceRoutine(void)
{
#ifdef LIBPORPOISE_PORT
	ARQRequest* request;
	ARQCallback callback;

	OSCheckAlarmQueue();
	request = __ARQHostPending;
	callback = __ARQHostCallback;
	__ARQHostPending = NULL;
	__ARQHostCallback = NULL;
	if (request != NULL && callback != NULL) {
		__ARQInvokeCallback(callback, request);
	}
	OSCheckAlarmQueue();
	return;
#endif
	if (__ARQCallbackHi) {
		__ARQInvokeCallback(
		    __ARQCallbackHi,
		    __ARQRequestPendingHi);
		__ARQRequestPendingHi = NULL;
		__ARQCallbackHi       = NULL;

	} else if (__ARQCallbackLo) {
		__ARQInvokeCallback(
		    __ARQCallbackLo,
		    __ARQRequestPendingLo);
		__ARQRequestPendingLo = NULL;
		__ARQCallbackLo       = NULL;
	}

	__ARQPopTaskQueueHi();

	if (__ARQRequestPendingHi == NULL) {
		__ARQServiceQueueLo();
	}
#ifdef LIBPORPOISE_PORT
	OSCheckAlarmQueue();
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000010
 */
void __ARQInitTempQueue(void)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000028
 */
void __ARQPushTempQueue(void)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 */
void ARQInit()
{
	if (__ARQ_init_flag == TRUE) {
		return;
	}

	__ARQRequestQueueHi = __ARQRequestQueueLo = NULL;
	__ARQChunkSize                            = ARQ_CHUNK_SIZE_DEFAULT;
	ARRegisterDMACallback(&__ARQInterruptServiceRoutine);
	__ARQRequestPendingHi = NULL;
	__ARQRequestPendingLo = NULL;
	__ARQCallbackHi       = NULL;
	__ARQCallbackLo       = NULL;
#ifdef LIBPORPOISE_PORT
	__ARQHostPending      = NULL;
	__ARQHostCallback     = NULL;
#endif

	__ARQ_init_flag = TRUE;
}

BOOL ARQCheckInit(void)
{
	return __ARQ_init_flag;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00000C
 */
void ARQReset(void)
{
#ifdef LIBPORPOISE_PORT
	BOOL enabled = OSDisableInterrupts();
	__ARQRequestQueueHi = __ARQRequestQueueLo = NULL;
	__ARQRequestTailHi = __ARQRequestTailLo = NULL;
	__ARQRequestPendingHi = __ARQRequestPendingLo = NULL;
	__ARQCallbackHi = __ARQCallbackLo = NULL;
	__ARQHostPending = NULL;
	__ARQHostCallback = NULL;
	__ARQChunkSize = ARQ_CHUNK_SIZE_DEFAULT;
	OSRestoreInterrupts(enabled);
#else
	TRAP_UNIMPLEMENTED;
#endif
}

/**
 * @TODO: Documentation
 */
void ARQPostRequest(ARQRequest* task, u32 owner, u32 type, u32 priority, u32 source, u32 dest, u32 length, ARQCallback callback)
{
	BOOL enabled;

#ifdef LIBPORPOISE_PORT
	OSCheckAlarmQueue();
	if (task == NULL) {
		return;
	}
	if (!__ARQ_init_flag) {
		ARQInit();
	}
#endif
	task->next   = NULL;
	task->owner  = owner;
	task->type   = type;
	task->priority = priority;
	task->source = source;
	task->dest   = dest;
	task->length = length;

	if (callback) {
		task->callback = callback;
	} else {
		task->callback = (ARQCallback)&__ARQCallbackHack;
	}

	enabled = OSDisableInterrupts();

#ifdef LIBPORPOISE_PORT
	/*
	 * Compatibility phase: requests execute immediately and synchronously.
	 * Priority and low-priority chunking are recorded but deliberately do not
	 * affect scheduling until ARQ is moved to a deterministic runtime queue.
	 */
	__ARQHostPending = task;
	__ARQHostCallback = callback;
	if (type == ARQ_TYPE_MRAM_TO_ARAM) {
		ARStartDMA(type, source, dest, length);
	} else {
		ARStartDMA(type, dest, source, length);
	}
	OSRestoreInterrupts(enabled);
	OSCheckAlarmQueue();
	return;
#endif

	switch (priority) {
	case ARQ_PRIORITY_LOW:
	{
		if (__ARQRequestQueueLo) {
			__ARQRequestTailLo->next = task;
		} else {
			__ARQRequestQueueLo = task;
		}
		__ARQRequestTailLo = task;
		break;
	}
	case ARQ_PRIORITY_HIGH:
	{
		if (__ARQRequestQueueHi) {
			__ARQRequestTailHi->next = task;
		} else {
			__ARQRequestQueueHi = task;
		}
		__ARQRequestTailHi = task;
		break;
	}
	}

	if ((__ARQRequestPendingHi == NULL) && (__ARQRequestPendingLo == NULL)) {
		__ARQPopTaskQueueHi();

		if (__ARQRequestPendingHi == NULL) {
			__ARQServiceQueueLo();
		}
	}

	OSRestoreInterrupts(enabled);
#ifdef LIBPORPOISE_PORT
	OSCheckAlarmQueue();
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000110
 */
void ARQRemoveRequest(void)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00011C
 */
void ARQRemoveOwnerRequest(void)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000048
 */
void ARQFlushQueue(void)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000020
 */
void ARQSetChunkSize(u32 size)
{
#ifdef LIBPORPOISE_PORT
	if (size >= ARQ_DMA_ALIGNMENT &&
	    (size & (ARQ_DMA_ALIGNMENT - 1U)) == 0) {
		__ARQChunkSize = size;
	}
#else
	TRAP_UNIMPLEMENTED;
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000008
 */
u32 ARQGetChunkSize(void)
{
#ifdef LIBPORPOISE_PORT
	return __ARQChunkSize;
#else
	TRAP_UNIMPLEMENTED;
	return 0;
#endif
}
