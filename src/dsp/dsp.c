#include <dolphin.h>
#include <dolphin/hw_regs.h>
#include <stddef.h>
#include <macros.h>

#include "__dsp.h"

#define BUILD_DATE "Dec 17 2001"
#define BUILD_TIME "18:25:00"

#ifdef LIBPORPOISE_PORT

DSPTaskInfo* __DSP_tmp_task;
DSPTaskInfo* __DSP_last_task;
DSPTaskInfo* __DSP_first_task;
DSPTaskInfo* __DSP_curr_task;
static BOOL HostDSPDispatching;

static BOOL DSPHostTaskIsQueued(DSPTaskInfo* task)
{
	DSPTaskInfo* current = __DSP_first_task;

	while (current != NULL) {
		if (current == task) {
			return TRUE;
		}
		current = current->next;
	}
	return FALSE;
}

static void DSPHostInsertTask(DSPTaskInfo* task)
{
	DSPTaskInfo* current;

	task->next = NULL;
	task->prev = NULL;
	if (__DSP_first_task == NULL) {
		__DSP_first_task = task;
		__DSP_last_task = task;
		return;
	}

	/* Match the SDK's stable priority ordering: lower values run first and
	 * equal-priority tasks remain in submission order. */
	current = __DSP_first_task;
	while (current != NULL && task->priority >= current->priority) {
		current = current->next;
	}

	if (current == NULL) {
		task->prev = __DSP_last_task;
		__DSP_last_task->next = task;
		__DSP_last_task = task;
		return;
	}

	task->next = current;
	task->prev = current->prev;
	current->prev = task;
	if (task->prev != NULL) {
		task->prev->next = task;
	} else {
		__DSP_first_task = task;
	}
}

static void DSPHostRemoveTask(DSPTaskInfo* task)
{
	if (task->prev != NULL) {
		task->prev->next = task->next;
	} else if (__DSP_first_task == task) {
		__DSP_first_task = task->next;
	}

	if (task->next != NULL) {
		task->next->prev = task->prev;
	} else if (__DSP_last_task == task) {
		__DSP_last_task = task->prev;
	}

	if (__DSP_curr_task == task) {
		__DSP_curr_task = NULL;
	}
	if (__DSP_tmp_task == task) {
		__DSP_tmp_task = NULL;
	}
	task->next = NULL;
	task->prev = NULL;
	task->flags = DSP_TASK_FLAG_CLEARALL;
	task->state = DSP_TASK_STATE_DONE;
}

static void DSPHostDispatchTasks(void)
{
	DSPTaskInfo* task;
	DSPCallback callback;

	HostDSPDispatching = TRUE;
	while (__DSP_first_task != NULL) {
		task = __DSP_first_task;
		__DSP_curr_task = task;
		task->state = DSP_TASK_STATE_RUN;

		callback = task->init_cb;
		if (callback != NULL) {
			callback(task);
		}

		/* The SDK invokes done_cb while the task is still RUN/ATTACHED, then
		 * removes it.  Reentrant submissions are queued by DSPAddTask and are
		 * dispatched after this callback returns. */
		callback = task->done_cb;
		if (callback != NULL) {
			callback(task);
		}

		DSPHostRemoveTask(task);
	}
	__DSP_curr_task = NULL;
	HostDSPDispatching = FALSE;
}

void __DSPHostInitDefault(void)
{
	__DSP_tmp_task = NULL;
	__DSP_last_task = NULL;
	__DSP_first_task = NULL;
	__DSP_curr_task = NULL;
	HostDSPDispatching = FALSE;
}

void DSPAssertInt(void)
{
}

void DSPSendMailToDSP(u32 mail)
{
	/* No host DSP consumes mail, so acknowledge it immediately. */
	(void)mail;
}

u32 DSPReadMailFromDSP(void)
{
	return 0;
}

u32 DSPCheckMailToDSP(void)
{
	return FALSE;
}

u32 DSPCheckMailFromDSP(void)
{
	return FALSE;
}

void DSPHalt(void)
{
}

void DSPReset(void)
{
	__DSPHostInitDefault();
}

DSPTaskInfo* DSPAddTask(DSPTaskInfo* task)
{
	if (task == NULL) {
		return NULL;
	}

	/* As in the SDK, a task already attached to the queue is not inserted a
	 * second time.  This also bounds accidental self-submission in callbacks. */
	if (DSPHostTaskIsQueued(task)) {
		return task;
	}

	/* There is no DSP worker on the host.  Model the observable SDK task
	 * lifecycle synchronously so readiness/completion waits remain bounded. */
	task->state = DSP_TASK_STATE_INIT;
	task->flags = DSP_TASK_FLAG_ATTACHED;
	DSPHostInsertTask(task);

	if (!HostDSPDispatching) {
		DSPHostDispatchTasks();
	}

	return task;
}

void __DSP_exec_task(DSPTaskInfo* current, DSPTaskInfo* next)
{
	(void)current;
	(void)next;
}

void __DSP_boot_task(DSPTaskInfo* task)
{
	(void)task;
}

void __DSP_remove_task(DSPTaskInfo* task)
{
	if (task != NULL && DSPHostTaskIsQueued(task)) {
		DSPHostRemoveTask(task);
	}
}

void __DSP_insert_task(DSPTaskInfo* task)
{
	if (task != NULL && !DSPHostTaskIsQueued(task)) {
		DSPHostInsertTask(task);
	}
}

void __DSP_add_task(DSPTaskInfo* task)
{
	(void)DSPAddTask(task);
}

void __DSP_debug_printf(const char* format, ...)
{
	(void)format;
}

void __DSPHandler(__OSInterrupt interrupt, OSContext* context)
{
	(void)interrupt;
	(void)context;
}

#else

u32 DSPCheckMailToDSP(void) { return (__DSPRegs[0] & (1 << 15)) >> 15; }

u32 DSPCheckMailFromDSP(void) { return (__DSPRegs[2] & (1 << 15)) >> 15; }

u32 DSPReadMailFromDSP(void) { return (__DSPRegs[2] << 16) | __DSPRegs[3]; }

void DSPSendMailToDSP(u32 mail)
{
	__DSPRegs[0] = mail >> 16;
	__DSPRegs[1] = mail & 0xFFFF;
}

// void DSPAssertInt(void)
// {
// 	BOOL old;
// 	u16 tmp;

// 	old          = OSDisableInterrupts();
// 	tmp          = __DSPRegs[5];
// 	tmp          = (tmp & ~0xA8) | 2;
// 	__DSPRegs[5] = tmp;
// 	OSRestoreInterrupts(old);
// }

#endif /* LIBPORPOISE_PORT */

// static int __DSP_init_flag;

// void DSPInit(void)
// {
// 	BOOL old;
// 	u16 tmp;

// 	__DSP_debug_printf("DSPInit(): Build Date: %s %s\n", BUILD_DATE,
// 	                   BUILD_TIME);

// 	if (__DSP_init_flag == 1)
// 		return;

// 	old = OSDisableInterrupts();
// 	__OSSetInterruptHandler(7, __DSPHandler);
// 	__OSUnmaskInterrupts(OS_INTERRUPTMASK_DSP_DSP);

// 	tmp          = __DSPRegs[5];
// 	tmp          = (tmp & ~0xA8) | 0x800;
// 	__DSPRegs[5] = tmp;

// 	tmp          = __DSPRegs[5];
// 	__DSPRegs[5] = tmp = tmp & ~0xAC;

// 	__DSP_first_task = __DSP_last_task = __DSP_curr_task = __DSP_tmp_task
// 	    = NULL;
// 	__DSP_init_flag = 1;

// 	OSRestoreInterrupts(old);
// }
