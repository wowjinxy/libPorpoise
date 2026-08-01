#include <dolphin/dsp.h>

namespace {

DSPTaskInfo sFirstTask;
DSPTaskInfo sNestedTask;
int sEvents[4];
int sEventCount;
bool sContractValid;

void RecordEvent(int event)
{
	if (sEventCount >= 4) {
		sContractValid = false;
		return;
	}
	sEvents[sEventCount++] = event;
}

void FirstInit(void* argument)
{
	auto* task = static_cast<DSPTaskInfo*>(argument);
	RecordEvent(1);
	if (task != &sFirstTask || task->state != DSP_TASK_STATE_RUN ||
	    task->flags != DSP_TASK_FLAG_ATTACHED ||
	    __DSP_first_task != task || __DSP_last_task != task ||
	    __DSP_curr_task != task) {
		sContractValid = false;
	}
}

void NestedInit(void* argument)
{
	auto* task = static_cast<DSPTaskInfo*>(argument);
	RecordEvent(3);
	if (task != &sNestedTask || task->state != DSP_TASK_STATE_RUN ||
	    task->flags != DSP_TASK_FLAG_ATTACHED ||
	    __DSP_first_task != task || __DSP_last_task != task ||
	    __DSP_curr_task != task) {
		sContractValid = false;
	}
}

void NestedDone(void* argument)
{
	auto* task = static_cast<DSPTaskInfo*>(argument);
	RecordEvent(4);
	if (task != &sNestedTask || task->state != DSP_TASK_STATE_RUN ||
	    task->flags != DSP_TASK_FLAG_ATTACHED ||
	    __DSP_first_task != task || __DSP_last_task != task ||
	    __DSP_curr_task != task) {
		sContractValid = false;
	}
}

void FirstDone(void* argument)
{
	auto* task = static_cast<DSPTaskInfo*>(argument);
	RecordEvent(2);
	if (task != &sFirstTask || task->state != DSP_TASK_STATE_RUN ||
	    task->flags != DSP_TASK_FLAG_ATTACHED ||
	    __DSP_first_task != task || __DSP_last_task != task ||
	    __DSP_curr_task != task) {
		sContractValid = false;
	}

	/* Completion callbacks may submit follow-up work, but SDK callback
	 * ordering requires it to remain queued until this callback returns. */
	if (DSPAddTask(&sNestedTask) != &sNestedTask ||
	    sNestedTask.state != DSP_TASK_STATE_INIT ||
	    sNestedTask.flags != DSP_TASK_FLAG_ATTACHED ||
	    __DSP_curr_task != &sFirstTask || __DSP_first_task != &sFirstTask ||
	    __DSP_last_task != &sNestedTask || sFirstTask.next != &sNestedTask ||
	    sNestedTask.prev != &sFirstTask) {
		sContractValid = false;
	}
}

}  // namespace

int main()
{
	DSPReset();
	if (DSPAddTask(nullptr) != nullptr) {
		return 1;
	}

	sFirstTask = {};
	sNestedTask = {};
	sFirstTask.init_cb = FirstInit;
	sFirstTask.done_cb = FirstDone;
	sFirstTask.priority = 0;
	sNestedTask.init_cb = NestedInit;
	sNestedTask.done_cb = NestedDone;
	sNestedTask.priority = 1;
	sEventCount = 0;
	sContractValid = true;

	if (DSPAddTask(&sFirstTask) != &sFirstTask) {
		return 2;
	}

	if (!sContractValid || sEventCount != 4 || sEvents[0] != 1 ||
	    sEvents[1] != 2 || sEvents[2] != 3 || sEvents[3] != 4) {
		return 3;
	}

	if (sFirstTask.state != DSP_TASK_STATE_DONE ||
	    sNestedTask.state != DSP_TASK_STATE_DONE ||
	    sFirstTask.flags != DSP_TASK_FLAG_CLEARALL ||
	    sNestedTask.flags != DSP_TASK_FLAG_CLEARALL ||
	    __DSP_tmp_task != nullptr || __DSP_first_task != nullptr ||
	    __DSP_last_task != nullptr || __DSP_curr_task != nullptr) {
		return 4;
	}

	DSPReset();
	return __DSP_tmp_task == nullptr && __DSP_first_task == nullptr &&
	               __DSP_last_task == nullptr && __DSP_curr_task == nullptr
	           ? 0
	           : 5;
}
