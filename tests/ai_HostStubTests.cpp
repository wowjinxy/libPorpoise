#include <dolphin/ai.h>
#include <dolphin/dsp.h>

namespace {

int sDMACallbackCount;
bool sDMACallbackSawCompletedState;
bool sRestartDMAFromCallback;

void DMACallback()
{
	++sDMACallbackCount;
	if (AIGetDMAEnableFlag() || AIGetDMABytesLeft() != 0) {
		sDMACallbackSawCompletedState = false;
	}

	if (sRestartDMAFromCallback) {
		sRestartDMAFromCallback = false;
		AIInitDMA(0x00200000U, 0x80U);
		AIStartDMA();
	}
}

void StreamCallback(u32)
{
}

bool TestInitializationAndDefaults()
{
	AIReset();
	if (AICheckInit()) {
		return false;
	}

	AIInit(nullptr);
	AIInit(nullptr);
	return AICheckInit() &&
	       AIGetDSPSampleRate() == AI_SAMPLERATE_32KHZ &&
	       AIGetStreamSampleRate() == AI_SAMPLERATE_48KHZ &&
	       AIGetStreamPlayState() == AI_STREAM_STOP &&
	       AIGetStreamSampleCount() == 0 &&
	       AIGetStreamTrigger() == 0 &&
	       AIGetStreamVolLeft() == 0 &&
	       AIGetStreamVolRight() == 0 &&
	       !AIGetDMAEnableFlag() &&
	       AIGetDMABytesLeft() == 0;
}

bool TestCallbacksAndControlState()
{
	if (AIRegisterDMACallback(DMACallback) != nullptr ||
	    AIRegisterDMACallback(nullptr) != DMACallback ||
	    AIRegisterStreamCallback(StreamCallback) != nullptr ||
	    AIRegisterStreamCallback(nullptr) != StreamCallback) {
		return false;
	}

	AISetDSPSampleRate(AI_SAMPLERATE_48KHZ);
	AISetStreamSampleRate(AI_SAMPLERATE_32KHZ);
	AISetStreamTrigger(0x12345678U);
	AISetStreamVolLeft(0x34);
	AISetStreamVolRight(0xAB);
	if (AIGetDSPSampleRate() != AI_SAMPLERATE_48KHZ ||
	    AIGetStreamSampleRate() != AI_SAMPLERATE_32KHZ ||
	    AIGetStreamTrigger() != 0x12345678U ||
	    AIGetStreamVolLeft() != 0x34 ||
	    AIGetStreamVolRight() != 0xAB) {
		return false;
	}

	AISetStreamPlayState(AI_STREAM_START);
	const u32 firstCount = AIGetStreamSampleCount();
	const u32 secondCount = AIGetStreamSampleCount();
	if (AIGetStreamPlayState() != AI_STREAM_START ||
	    secondCount != firstCount + 1) {
		return false;
	}
	AISetStreamPlayState(AI_STREAM_STOP);
	if (AIGetStreamSampleCount() != secondCount + 1) {
		return false;
	}
	AIResetStreamSampleCount();
	return AIGetStreamSampleCount() == 0;
}

bool TestBoundedDMAState()
{
	sDMACallbackCount = 0;
	sDMACallbackSawCompletedState = true;
	sRestartDMAFromCallback = true;
	if (AIRegisterDMACallback(DMACallback) != nullptr) {
		return false;
	}

	AIInitDMA(0x00123460U, 0x100U);
	if (AIGetDMAStartAddr() != 0x00123460U ||
	    AIGetDMALength() != 0x100U ||
	    AIGetDMABytesLeft() != 0x100U ||
	    AIGetDMAEnableFlag()) {
		return false;
	}

	AIStartDMA();
	/* The first completion callback starts another transfer.  Completion must
	 * publish the idle state before invoking it and must not erase that newly
	 * started transfer on return. */
	if (!AIGetDMAEnableFlag() || sDMACallbackCount != 1 ||
	    !sDMACallbackSawCompletedState ||
	    AIGetDMAStartAddr() != 0x00200000U ||
	    AIGetDMALength() != 0x80U ||
	    AIGetDMABytesLeft() != 0x80U || sDMACallbackCount != 2 ||
	    AIGetDMAEnableFlag()) {
		return false;
	}

	/* Repeated observation of an already-completed transfer must not deliver
	 * the callback again. */
	if (AIGetDMABytesLeft() != 0 || AIGetDMAEnableFlag() ||
	    sDMACallbackCount != 2) {
		return false;
	}

	AIStartDMA();
	if (AIGetDMABytesLeft() != 0x80U ||
	    AIGetDMABytesLeft() != 0 || AIGetDMAEnableFlag() ||
	    sDMACallbackCount != 3) {
		return false;
	}

	AIStartDMA();
	AIStopDMA();
	if (AIGetDMAEnableFlag() || AIGetDMABytesLeft() != 0 ||
	    sDMACallbackCount != 3) {
		return false;
	}

	return AIRegisterDMACallback(nullptr) == DMACallback;
}

bool TestDSPStub()
{
	DSPTaskInfo task = {};
	DSPInit();
	DSPAssertInt();
	DSPSendMailToDSP(0xDEADBEEFU);
	if (DSPCheckMailToDSP() != FALSE ||
	    DSPCheckMailFromDSP() != FALSE ||
	    DSPReadMailFromDSP() != 0 ||
	    DSPAddTask(&task) != &task ||
	    task.state != DSP_TASK_STATE_DONE ||
	    task.flags != DSP_TASK_FLAG_CLEARALL) {
		return false;
	}

	DSPHalt();
	DSPReset();
	return __DSP_first_task == nullptr &&
	       __DSP_last_task == nullptr &&
	       __DSP_curr_task == nullptr &&
	       __DSP_tmp_task == nullptr;
}

}  // namespace

int main()
{
	if (!TestInitializationAndDefaults()) {
		return 1;
	}
	if (!TestCallbacksAndControlState()) {
		return 2;
	}
	if (!TestBoundedDMAState()) {
		return 3;
	}
	if (!TestDSPStub()) {
		return 4;
	}

	AIReset();
	return AICheckInit() ? 5 : 0;
}
