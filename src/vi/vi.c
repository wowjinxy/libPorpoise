#include <dolphin/vi.h>
#include <dolphin/gx.h>
#include <dolphin/hw_regs.h>
#include <dolphin/os.h>
#include <dolphin/si.h>
#include <stddef.h>
#ifdef LIBPORPOISE_PORT
#include <simulator/sim.h>
#include <simulator/sim_host_Benchmark.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_timer.h>

extern void __GXHostServiceFifoBreakpoint(void);
#if defined(__GNUC__)
extern void __GXHostApplyCopyClear(void) __attribute__((weak));
extern BOOL SIM_HostReleaseRenderContext(void) __attribute__((weak));
extern BOOL SIM_HostAcquireRenderContext(void) __attribute__((weak));
#else
extern void __GXHostApplyCopyClear(void);
#endif

#define HOST_RENDER_HANDOFF_TIMEOUT_MS 5000U

typedef enum VIHostRenderHandoffState {
	VI_HOST_RENDER_HANDOFF_IDLE,
	VI_HOST_RENDER_HANDOFF_REQUESTED,
	VI_HOST_RENDER_HANDOFF_RELEASED,
	VI_HOST_RENDER_HANDOFF_RELEASE_FAILED,
	VI_HOST_RENDER_HANDOFF_ROLLBACK_REQUESTED,
	VI_HOST_RENDER_HANDOFF_ROLLED_BACK,
	VI_HOST_RENDER_HANDOFF_TRANSFERRED,
	VI_HOST_RENDER_HANDOFF_RETURN_PENDING,
	VI_HOST_RENDER_HANDOFF_FATAL,
} VIHostRenderHandoffState;

static BOOL hostRetraceInProgress;
static BOOL hostCopyRetracePendingWait;
static BOOL hostDisplayCopyPending;
static BOOL hostTextureCopyAwaitingDraw;
static BOOL hostRetraceRuntimeInitialized;
static SDL_threadID hostRetraceOwnerThread;
static SDL_threadID hostRetracePreviousOwnerThread;
static SDL_threadID hostReturnedAlarmOwnerThread;
static SDL_threadID hostRenderHandoffRequester;
static SDL_mutex* hostRenderHandoffMutex;
static SDL_cond* hostRenderHandoffCondition;
static OSAlarm hostRenderHandoffAlarm;
static VIHostRenderHandoffState hostRenderHandoffState;
static Uint64 hostRetraceDeadline;
static Uint64 hostRetraceRemainder;
static Uint64 hostRetraceFrequency;
static u32 hostRetraceRateNumerator;
static u32 hostRetraceRateDenominator;
#endif

// Useful macros.
#define CLAMP(x, l, h)    (((x) > (h)) ? (h) : (((x) < (l)) ? (l) : (x)))
#define MIN(a, b)         (((a) < (b)) ? (a) : (b))
#define MAX(a, b)         (((a) > (b)) ? (a) : (b))
#define IS_LOWER_16MB(x)  ((x) < 16 * 1024 * 1024)
#define ToPhysical(fb)    (u32)(((u32)(fb)) & 0x3FFFFFFF)
#define ONES(x)           ((1 << (x)) - 1)
#define VI_BITMASK(index) (1ull << (63 - (index)))

static vu32 retraceCount;
#if defined(VERSION_GPIE01_00) || defined(VERSION_GPIJ01_01)
static vu32 changeMode; // This exists up here for some reason.
#endif
static u32 flushFlag;
static OSThreadQueue retraceQueue;
static VIRetraceCallback PreCB;
static VIRetraceCallback PostCB;
static u32 encoderType;

static s16 displayOffsetH;
static s16 displayOffsetV;

#if defined(VERSION_GPIE01_00) || defined(VERSION_GPIJ01_01)
static vu64 changed;
#else
static vu32 changeMode;
static vu64 changed;
static vu32 shdwChangeMode;
#endif
static vu64 shdwChanged;

static u32 FBSet;

static vu16 regs[59];
static vu16 shdwRegs[59];

static VIPositionInfo HorVer;

#if OS_BUILD_VERSION >= 20011002L
static VITimingInfo* CurrTiming;
static s32 CurrTvMode;
#endif

// clang-format off
static VITimingInfo timing[] = {
	{ // NTSC INT
		6, 240, 24, 25, 3, 2, 12, 13, 12, 13, 520, 519, 520, 519, 525, 429, 64, 71, 105, 162, 373, 122, 412,
	},
	{ // NTSC DS
		6, 240, 24, 24, 4, 4, 12, 12, 12, 12, 520, 520, 520, 520, 526, 429, 64, 71, 105, 162, 373, 122, 412,
	},
	{ // PAL INT
		5, 287, 35, 36, 1, 0, 13, 12, 11, 10, 619, 618, 617, 620, 625, 432, 64, 75, 106, 172, 380, 133, 420,
	},
	{ // PAL DS
#if OS_BUILD_VERSION >= 20011217L
		5, 287, 33, 33, 2, 2, 13, 11, 13, 11, 619, 621, 619, 621, 624, 432, 64, 75, 106, 172, 380, 133, 420,
#else
		5, 287, 35, 35, 2, 2, 13, 11, 13, 11, 619, 621, 619, 621, 626, 432, 64, 75, 106, 172, 380, 133, 420,
#endif
	},
	{ // MPAL INT
		6, 240, 24, 25, 3, 2, 16, 15, 14, 13, 518, 517, 516, 519, 525, 429, 64, 78, 112, 162, 373, 122, 412,
	},
	{ // MPAL DS
		6, 240, 24, 24, 4, 4, 16, 14, 16, 14, 518, 520, 518, 520, 526, 429, 64, 78, 112, 162, 373, 122, 412,
	},
	{ // NTSC PRO
		12, 480, 48, 48, 6, 6, 24, 24, 24, 24, 1038, 1038, 1038, 1038, 1050, 429, 64, 71, 105, 162, 373, 122, 412,
	},
#if OS_BUILD_VERSION >= 20011112L
	{ // NTSC 3D
		12, 480, 44, 44, 10, 10, 24, 24, 24, 24, 1038, 1038, 1038, 1038, 1050, 429, 64, 71, 105, 168, 379, 122, 412,
	},
#endif
};
// clang-format on

static u16 taps[25] = { 496, 476, 430, 372, 297, 219, 142, 70, 12, 226, 203, 192, 196, 207, 222, 236, 252, 8, 15, 19, 19, 15, 12, 8, 1 };

// forward declaring statics
static u32 getCurrentFieldEvenOdd();

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000008
 */
s32 getEncoderType(void)
{
	return 1;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00005C (Matching by size)
 */
static int cntlzd(u64 bit)
{
	u32 hi, lo;
	int value;

#ifdef LIBPORPOISE_PORT
	if (bit == 0) {
		return 64;
	}
	return __builtin_clzll(bit);
#else
	hi    = (u32)(bit >> 32);
	lo    = (u32)(bit & 0xFFFFFFFF);
	value = __mwerks_cntlzw(hi);

	if (value < 32) {
		return value;
	}

	return (32 + __mwerks_cntlzw(lo));
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000108
 */
static BOOL VISetRegs(void)
{
	int regIndex;

#if defined(VERSION_GPIE01_00) || defined(VERSION_GPIJ01_01)
	if (!((changeMode == 1) && (getCurrentFieldEvenOdd() == 0)))
#else
	if (!((shdwChangeMode == 1) && (getCurrentFieldEvenOdd() == 0)))
#endif
	{
		while (shdwChanged) {
			regIndex           = cntlzd(shdwChanged);
			__VIRegs[regIndex] = shdwRegs[regIndex];
			shdwChanged &= ~(VI_BITMASK(regIndex));
		}

#if defined(VERSION_GPIE01_00) || defined(VERSION_GPIJ01_01)
		changeMode = 0;
#else
		shdwChangeMode = 0;
#endif

#if OS_BUILD_VERSION >= 20011002L
		CurrTiming = HorVer.timing;
#endif
#if OS_BUILD_VERSION >= 20011112L
		CurrTvMode = HorVer.tv;
#endif

		return TRUE;
	}
	return FALSE;
}

/**
 * @TODO: Documentation
 */
static void __VIRetraceHandler(__OSInterrupt interrupt, OSContext* context)
{
	OSContext exceptionContext;
	u16 viReg;
	u32 inter = 0;

	viReg = __VIRegs[VI_DISP_INT_0];
	if (viReg & 0x8000) {
		__VIRegs[VI_DISP_INT_0] = (u16)(viReg & ~0x8000);
		inter |= 1;
	}

	viReg = __VIRegs[VI_DISP_INT_1];
	if (viReg & 0x8000) {
		__VIRegs[VI_DISP_INT_1] = (u16)(viReg & ~0x8000);
		inter |= 2;
	}

	viReg = __VIRegs[VI_DISP_INT_2];
	if (viReg & 0x8000) {
		__VIRegs[VI_DISP_INT_2] = (u16)(viReg & ~0x8000);
		inter |= 4;
	}

	viReg = __VIRegs[VI_DISP_INT_3];
	if (viReg & 0x8000) {
		__VIRegs[VI_DISP_INT_3] = (u16)(viReg & ~0x8000);
		inter |= 8;
	}

	if ((inter & 4) || (inter & 8)) {
		OSSetCurrentContext(context);
		return;
	}

	retraceCount++;

	OSClearContext(&exceptionContext);
	OSSetCurrentContext(&exceptionContext);
	if (PreCB) {
		(*PreCB)(retraceCount);
	}

	if (flushFlag) {
		if (VISetRegs()) {
			flushFlag = 0;

#if OS_BUILD_VERSION >= 20011217L
			SIRefreshSamplingRate();
#elif OS_BUILD_VERSION >= 20011002L
			__PADRefreshSamplingRate();
#endif
		}
	}

	if (PostCB) {
		OSClearContext(&exceptionContext);
		(*PostCB)(retraceCount);
	}

	OSWakeupThread(&retraceQueue);
	OSClearContext(&exceptionContext);
	OSSetCurrentContext(context);
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000044 (Matching by size)
 */
VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback callback)
{
	BOOL interrupt;
	VIRetraceCallback oldCallback;

	oldCallback = PreCB;

	interrupt = OSDisableInterrupts();
	PreCB     = callback;
	OSRestoreInterrupts(interrupt);

	return oldCallback;
}

/**
 * @TODO: Documentation
 */
VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback callback)
{
	BOOL interrupt;
	VIRetraceCallback oldCallback;

	oldCallback = PostCB;

	interrupt = OSDisableInterrupts();
	PostCB    = callback;
	OSRestoreInterrupts(interrupt);

	return oldCallback;
}

#pragma dont_inline on

/**
 * @TODO: Documentation
 */
static VITimingInfo* getTiming(VITVMode mode)
{
	switch (mode) {
	case VI_TVMODE_NTSC_INT:
	{
		return &timing[0];
	}
	case VI_TVMODE_NTSC_DS:
	{
		return &timing[1];
	}

	case VI_TVMODE_PAL_INT:
	{
		return &timing[2];
	}
	case VI_TVMODE_PAL_DS:
	{
		return &timing[3];
	}

#if OS_BUILD_VERSION >= 20011112L
	case VI_TVMODE_EURGB60_INT:
	{
		return &timing[0];
	}
	case VI_TVMODE_EURGB60_DS:
	{
		return &timing[1];
	}
#endif

	case VI_TVMODE_MPAL_INT:
	{
		return &timing[4];
	}
	case VI_TVMODE_MPAL_DS:
	{
		return &timing[5];
	}

	case VI_TVMODE_NTSC_PROG:
	{
		return &timing[6];
	}

#if OS_BUILD_VERSION >= 20011002L
#if OS_BUILD_VERSION >= 20011112L
	case VI_TVMODE_NTSC_3D:
	{
		return &timing[7];
	}
#endif
	case VI_TVMODE_DEBUG_PAL_INT:
	{
		return &timing[2];
	}
	case VI_TVMODE_DEBUG_PAL_DS:
	{
		return &timing[3];
	}
#endif
	}

	return NULL;
}

#pragma dont_inline reset

/**
 * @TODO: Documentation
 */
void __VIInit(VITVMode mode)
{
	VITimingInfo* tm;
	u32 nonInter;
	vu32 a;
	u32 tv, tvForReg;

	u16 hct, vct;

	nonInter = mode & 2;
	tv       = (u32)mode >> 2;

	#ifndef LIBPORPOISE_PORT
	*(u32*)OSPhysicalToCached(0xCC) = tv;
	#endif

	tm = getTiming(mode);

	__VIRegs[VI_DISP_CONFIG] = 2;
	for (a = 0; a < 1000; a++) {
		;
	}

	__VIRegs[VI_DISP_CONFIG] = 0;

	__VIRegs[VI_HORIZ_TIMING_0U] = tm->hlw << 0;
	__VIRegs[VI_HORIZ_TIMING_0L] = (tm->hce << 0) | (tm->hcs << 8);

	__VIRegs[VI_HORIZ_TIMING_1U] = (tm->hsy << 0) | ((tm->hbe640 & ((1 << 9) - 1)) << 7);
	__VIRegs[VI_HORIZ_TIMING_1L] = ((tm->hbe640 >> 9) << 0) | (tm->hbs640 << 1);

	__VIRegs[VI_VERT_TIMING] = (tm->equ << 0) | (0 << 4);

	__VIRegs[VI_VERT_TIMING_ODD_U] = (tm->prbOdd + tm->acv * 2 - 2) << 0;
	__VIRegs[VI_VERT_TIMING_ODD]   = tm->psbOdd + 2 << 0;

	__VIRegs[VI_VERT_TIMING_EVEN_U] = (tm->prbEven + tm->acv * 2 - 2) << 0;
	__VIRegs[VI_VERT_TIMING_EVEN]   = tm->psbEven + 2 << 0;

	__VIRegs[VI_BBI_ODD_U] = (tm->bs1 << 0) | (tm->be1 << 5);
	__VIRegs[VI_BBI_ODD]   = (tm->bs3 << 0) | (tm->be3 << 5);

	__VIRegs[VI_BBI_EVEN_U] = (tm->bs2 << 0) | (tm->be2 << 5);
	__VIRegs[VI_BBI_EVEN]   = (tm->bs4 << 0) | (tm->be4 << 5);

	__VIRegs[VI_HSW] = (40 << 0) | (40 << 8);

	__VIRegs[VI_DISP_INT_1U] = 1;
	__VIRegs[VI_DISP_INT_1]  = (1 << 0) | (1 << 12) | (0 << 15);

	hct                      = (tm->hlw + 1);
	vct                      = (tm->numHalfLines / 2 + 1) | (1 << 12) | (0 << 15);
	__VIRegs[VI_DISP_INT_0U] = hct << 0;
	__VIRegs[VI_DISP_INT_0]  = vct;

#if OS_BUILD_VERSION >= 20011112L
	if (mode != VI_TVMODE_NTSC_PROG && mode != VI_TVMODE_NTSC_3D)
#else
	if (mode != VI_TVMODE_NTSC_PROG)
#endif
	{
		__VIRegs[VI_DISP_CONFIG] = (1 << 0) | (0 << 1) | (nonInter << 2) | (0 << 3) | (0 << 4) | (0 << 6) | (tv << 8);
		__VIRegs[VI_CLOCK_SEL]   = 0;

	} else {
		__VIRegs[VI_DISP_CONFIG] = (1 << 0) | (0 << 1) | (1 << 2) | (0 << 3) | (0 << 4) | (0 << 6) | (tv << 8);
		__VIRegs[VI_CLOCK_SEL]   = 1;
	}
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000160 (Matching by size)
 */
static void AdjustPosition(u16 acv)
{
	s32 coeff, frac;

	HorVer.adjDispPosX = (u16)CLAMP((s16)HorVer.dispPosX + displayOffsetH, 0, 720 - HorVer.dispSizeX);

	coeff = (HorVer.xfbMode == VI_XFBMODE_SF) ? 2 : 1;
	frac  = HorVer.dispPosY & 1;

	HorVer.adjDispPosY = (u16)MAX((s16)HorVer.dispPosY + displayOffsetV, frac);

	HorVer.adjDispSizeY = (u16)(HorVer.dispSizeY + MIN((s16)HorVer.dispPosY + displayOffsetV - frac, 0)
	                            - MAX((s16)HorVer.dispPosY + (s16)HorVer.dispSizeY + displayOffsetV - ((s16)acv * 2 - frac), 0));

	HorVer.adjPanPosY = (u16)(HorVer.panPosY - MIN((s16)HorVer.dispPosY + displayOffsetV - frac, 0) / coeff);

	HorVer.adjPanSizeY = (u16)(HorVer.panSizeY + MIN((s16)HorVer.dispPosY + displayOffsetV - frac, 0) / coeff
	                           - MAX((s16)HorVer.dispPosY + (s16)HorVer.dispSizeY + displayOffsetV - ((s16)acv * 2 - frac), 0) / coeff);
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00003C (Matching by size)
 */
static void ImportAdjustingValues(void)
{
	displayOffsetH = __OSLockSram()->displayOffsetH;
	displayOffsetV = 0;
	__OSUnlockSram(FALSE);
}

#ifdef LIBPORPOISE_PORT
static BOOL __VIHostReleaseSimulatorContext(void)
{
#if defined(__GNUC__)
	if (SIM_HostReleaseRenderContext == NULL) {
		return TRUE;
	}
#endif
	return SIM_HostReleaseRenderContext();
}

static BOOL __VIHostAcquireSimulatorContext(void)
{
#if defined(__GNUC__)
	if (SIM_HostAcquireRenderContext == NULL) {
		return TRUE;
	}
#endif
	return SIM_HostAcquireRenderContext();
}

static void __VIHostWakeRetraceWaiters(void)
{
	/*
	 * Do not take the global scheduler lock here. The previous owner may be
	 * inside a handoff callback while still holding retraceQueue.hostMutex;
	 * locking scheduler then queue would invert its queue -> scheduler wake
	 * path. Taking only the queue mutex also closes the signal-before-sleep
	 * race because SDL_CondWait releases that same mutex atomically.
	 */
	SDL_LockMutex(retraceQueue.hostMutex);
	SDL_CondBroadcast(retraceQueue.hostCondition);
	SDL_UnlockMutex(retraceQueue.hostMutex);
}

/*
 * Context changes are only committed while the old render thread is parked
 * in a host wait, or after its current retrace has fully presented. This keeps
 * an alarm serviced from inside a retrace from detaching GL before SIM_Render.
 */
static void __VIHostRenderContextAlarm(OSAlarm* alarm, OSContext* context);
static void __VIHostRenderOwnerThreadWillExit(SDL_threadID exitingThread);
static void __VIHostWaitForHandoffState(
	VIHostRenderHandoffState pendingState,
	Uint64 deadline);

static BOOL __VIHostCurrentThreadOwnsReadyContext(void)
{
	SDL_threadID currentThread = SDL_ThreadID();
	BOOL acquired = TRUE;
	BOOL alarmTransferred = TRUE;
	BOOL enabled;
	BOOL isRenderThread;
	BOOL reportAlarmFailure = FALSE;
	BOOL reportFailure = FALSE;

	enabled = OSDisableInterrupts();
	SDL_LockMutex(hostRenderHandoffMutex);
	if (currentThread == hostRetraceOwnerThread &&
	    hostRenderHandoffState ==
	        VI_HOST_RENDER_HANDOFF_RETURN_PENDING) {
		/*
		 * The exiting owner detached GL on its own thread. Only the returned
		 * owner may attach it again, so defer that half of the handback until
		 * this thread next reaches a VI/GX ownership boundary.
		 */
		acquired = __VIHostAcquireSimulatorContext();
		if (acquired) {
			alarmTransferred = __OSHostTransferAlarmThread(
				hostReturnedAlarmOwnerThread);
		}
		hostRenderHandoffState =
			acquired && alarmTransferred
			    ? VI_HOST_RENDER_HANDOFF_IDLE
			    : VI_HOST_RENDER_HANDOFF_FATAL;
		if (acquired && alarmTransferred) {
			hostReturnedAlarmOwnerThread = 0;
		}
		SDL_CondBroadcast(hostRenderHandoffCondition);
		reportFailure = !acquired;
		reportAlarmFailure = acquired && !alarmTransferred;
	}
	isRenderThread =
		acquired && alarmTransferred &&
		hostRenderHandoffState == VI_HOST_RENDER_HANDOFF_IDLE &&
		currentThread == hostRetraceOwnerThread;
	SDL_UnlockMutex(hostRenderHandoffMutex);
	OSRestoreInterrupts(enabled);

	if (reportFailure) {
		OSReport("libPorpoise VI: the returned render owner could not "
		         "reacquire its context.\n");
	} else if (reportAlarmFailure) {
		OSReport("libPorpoise VI: the returned render owner could not "
		         "reacquire alarm dispatch.\n");
	}
	return isRenderThread;
}

static void __VIHostGetRetraceRate(u32* numerator, u32* denominator)
{
	/* PAL and DEBUG_PAL scan at 50 fields per second. The remaining
	 * GameCube modes use the NTSC-family 60000/1001 field cadence. */
	if (HorVer.tv == VI_PAL || HorVer.tv == VI_DEBUG_PAL) {
		*numerator = 50u;
		*denominator = 1u;
	} else {
		*numerator = 60000u;
		*denominator = 1001u;
	}
}

static void __VIHostResetRetracePacing(void)
{
	hostRetraceDeadline = 0u;
	hostRetraceRemainder = 0u;
	hostRetraceFrequency = 0u;
	hostRetraceRateNumerator = 0u;
	hostRetraceRateDenominator = 0u;
}

static void __VIHostPaceRetrace(void)
{
	Uint64 frequency;
	Uint64 now;
	Uint64 scaledPeriod;
	Uint64 period;
	Uint64 target;
	u32 rateNumerator;
	u32 rateDenominator;

	frequency = SDL_GetPerformanceFrequency();
	if (frequency == 0u) {
		return;
	}
	__VIHostGetRetraceRate(&rateNumerator, &rateDenominator);
	now = SDL_GetPerformanceCounter();

	/* The first host retrace, a timer source change, or a TV-mode change
	 * establishes a phase without adding an artificial startup frame. */
	if (hostRetraceDeadline == 0u ||
	    hostRetraceFrequency != frequency ||
	    hostRetraceRateNumerator != rateNumerator ||
	    hostRetraceRateDenominator != rateDenominator) {
		hostRetraceDeadline = now;
		hostRetraceRemainder = 0u;
		hostRetraceFrequency = frequency;
		hostRetraceRateNumerator = rateNumerator;
		hostRetraceRateDenominator = rateDenominator;
		return;
	}

	/* Carry the fractional performance-counter tick so the long-term rate is
	 * 60000/1001 rather than a rounded integer approximation. */
	scaledPeriod = frequency * (Uint64)rateDenominator;
	period = scaledPeriod / (Uint64)rateNumerator;
	hostRetraceRemainder += scaledPeriod % (Uint64)rateNumerator;
	if (hostRetraceRemainder >= (Uint64)rateNumerator) {
		period++;
		hostRetraceRemainder -= (Uint64)rateNumerator;
	}
	if (period == 0u) {
		period = 1u;
	}
	target = hostRetraceDeadline + period;
	if (target < hostRetraceDeadline) {
		hostRetraceDeadline = now;
		hostRetraceRemainder = 0u;
		return;
	}

	if (now < target) {
		const Uint64 remaining = target - now;
		Uint64 delayMs =
			(remaining * 1000u + frequency - 1u) / frequency;

		/* Use one rounded-up wait. Splitting off a final SDL_Delay(1) can
		 * consume a second scheduler quantum on Windows and turn 60 Hz into
		 * roughly 30 Hz. SDL_Delay waits at least the requested duration. */
		if (delayMs > 0xffffffffu) {
			delayMs = 0xffffffffu;
		}
		SDL_Delay((Uint32)delayMs);
		now = SDL_GetPerformanceCounter();
	}

	/* Preserve the hardware phase after ordinary timer overshoot. If work is
	 * a complete retrace late, retain at most one interval of timing credit.
	 * This lets one fast frame follow a slow frame without reducing an already
	 * sub-60 workload, while the following deadline restores the ceiling and
	 * prevents an unbounded catch-up burst after a long stall. */
	if (now - target >= period) {
		hostRetraceDeadline = now >= period ? now - period : now;
		hostRetraceRemainder = 0u;
	} else {
		hostRetraceDeadline = target;
	}
}

static void __VIHostServiceRenderContextRequest(BOOL renderSafePoint)
{
	Uint64 deadline;
	BOOL enabled;
	BOOL contextReady;

	enabled = OSDisableInterrupts();
	SDL_LockMutex(hostRenderHandoffMutex);
	if (SDL_ThreadID() != hostRetraceOwnerThread ||
	    hostRetraceInProgress) {
		SDL_UnlockMutex(hostRenderHandoffMutex);
		OSRestoreInterrupts(enabled);
		return;
	}

	if (hostRenderHandoffState == VI_HOST_RENDER_HANDOFF_REQUESTED) {
		if (!renderSafePoint &&
		    !__OSHostIsBlockingWaitSafePoint()) {
			/* Retry later; a direct OSCheckAlarmQueue is not a GL boundary. */
			OSSetAlarm(
			    &hostRenderHandoffAlarm,
			    OSMillisecondsToTicks(1),
			    __VIHostRenderContextAlarm);
			SDL_UnlockMutex(hostRenderHandoffMutex);
			OSRestoreInterrupts(enabled);
			return;
		}
		/* The request may have arrived after this retrace's alarm check. */
		OSCancelAlarm(&hostRenderHandoffAlarm);
		contextReady = __VIHostReleaseSimulatorContext();
		hostRenderHandoffState =
		    contextReady
		        ? VI_HOST_RENDER_HANDOFF_RELEASED
		        : VI_HOST_RENDER_HANDOFF_RELEASE_FAILED;
		SDL_CondBroadcast(hostRenderHandoffCondition);
		if (contextReady) {
			/*
			 * Do not let the former owner leave this boundary between GL
			 * release and either a committed transfer or a completed rollback.
			 */
			deadline =
			    SDL_GetTicks64() + HOST_RENDER_HANDOFF_TIMEOUT_MS;
			__VIHostWaitForHandoffState(
			    VI_HOST_RENDER_HANDOFF_RELEASED,
			    deadline);
		}
	}

	if (hostRenderHandoffState ==
	    VI_HOST_RENDER_HANDOFF_ROLLBACK_REQUESTED) {
		OSCancelAlarm(&hostRenderHandoffAlarm);
		contextReady = __VIHostAcquireSimulatorContext();
		hostRenderHandoffState =
		    contextReady
		        ? VI_HOST_RENDER_HANDOFF_ROLLED_BACK
		        : VI_HOST_RENDER_HANDOFF_FATAL;
		SDL_CondBroadcast(hostRenderHandoffCondition);
		if (contextReady) {
			deadline =
			    SDL_GetTicks64() + HOST_RENDER_HANDOFF_TIMEOUT_MS;
			__VIHostWaitForHandoffState(
			    VI_HOST_RENDER_HANDOFF_ROLLED_BACK,
			    deadline);
		}
	}

	if (hostRenderHandoffState == VI_HOST_RENDER_HANDOFF_RELEASED) {
		/* The requester vanished after release; restore a usable owner. */
		contextReady = __VIHostAcquireSimulatorContext();
		hostRenderHandoffState =
		    contextReady
		        ? VI_HOST_RENDER_HANDOFF_IDLE
		        : VI_HOST_RENDER_HANDOFF_FATAL;
		hostRenderHandoffRequester = 0;
		SDL_CondBroadcast(hostRenderHandoffCondition);
	} else if (hostRenderHandoffState ==
	           VI_HOST_RENDER_HANDOFF_ROLLED_BACK) {
		/* The requester vanished after rollback; the old owner is usable. */
		hostRenderHandoffState = VI_HOST_RENDER_HANDOFF_IDLE;
		hostRenderHandoffRequester = 0;
		SDL_CondBroadcast(hostRenderHandoffCondition);
	} else if (hostRenderHandoffState ==
	           VI_HOST_RENDER_HANDOFF_TRANSFERRED) {
		/*
		 * Acknowledge that the old owner has left its release boundary. The
		 * new owner must not enter VI while an outer OS wait still holds a
		 * queue mutex that the retrace path needs.
		 */
		hostRenderHandoffState = VI_HOST_RENDER_HANDOFF_IDLE;
		SDL_CondBroadcast(hostRenderHandoffCondition);
	}
	SDL_UnlockMutex(hostRenderHandoffMutex);
	OSRestoreInterrupts(enabled);
}

static void __VIHostRenderOwnerThreadWillExit(
	SDL_threadID exitingThread)
{
	SDL_threadID previousOwner;
	BOOL contextReleased;
	BOOL enabled;

	if (!hostRetraceRuntimeInitialized ||
	    hostRenderHandoffMutex == NULL) {
		return;
	}

	/*
	 * If an adoption raced the owner's natural return, the return itself is a
	 * safe GL boundary. Let the normal transactional handoff finish first.
	 */
	enabled = OSDisableInterrupts();
	SDL_LockMutex(hostRenderHandoffMutex);
	previousOwner = hostRetracePreviousOwnerThread;
	if (exitingThread == hostRetraceOwnerThread &&
	    hostRenderHandoffState == VI_HOST_RENDER_HANDOFF_REQUESTED) {
		SDL_UnlockMutex(hostRenderHandoffMutex);
		OSRestoreInterrupts(enabled);
		__VIHostServiceRenderContextRequest(TRUE);

		enabled = OSDisableInterrupts();
		SDL_LockMutex(hostRenderHandoffMutex);
		if (hostRetraceOwnerThread != exitingThread) {
			/* Do not leave a successful new lease pointing back at a dead owner. */
			if (hostRetracePreviousOwnerThread == exitingThread) {
				hostRetracePreviousOwnerThread = previousOwner;
			}
			SDL_UnlockMutex(hostRenderHandoffMutex);
			OSRestoreInterrupts(enabled);
			return;
		}
	}

	if (exitingThread != hostRetraceOwnerThread) {
		/* A fallback that exits before the active owner cannot receive a lease. */
		if (exitingThread == hostRetracePreviousOwnerThread) {
			hostRetracePreviousOwnerThread = 0;
		}
		SDL_UnlockMutex(hostRenderHandoffMutex);
		OSRestoreInterrupts(enabled);
		return;
	}

	if (hostRenderHandoffState != VI_HOST_RENDER_HANDOFF_IDLE ||
	    hostRetraceInProgress ||
	    previousOwner == 0 ||
	    previousOwner == exitingThread) {
		hostRetraceOwnerThread = 0;
		hostRetracePreviousOwnerThread = 0;
		hostReturnedAlarmOwnerThread = 0;
		hostRenderHandoffRequester = 0;
		hostRenderHandoffState = VI_HOST_RENDER_HANDOFF_FATAL;
		SDL_CondBroadcast(hostRenderHandoffCondition);
		SDL_UnlockMutex(hostRenderHandoffMutex);
		OSRestoreInterrupts(enabled);
		__VIHostWakeRetraceWaiters();
		OSReport("libPorpoise VI: the render owner exited without a live "
		         "previous owner to receive its context.\n");
		return;
	}

	/*
	 * Native GL release must occur on the exiting owner. Keep alarm dispatch
	 * parked on that now-inactive ID until the returned owner has attached GL;
	 * otherwise a due alarm could issue GX work from its wake path too early.
	 */
	contextReleased = __VIHostReleaseSimulatorContext();
	if (contextReleased) {
		hostRetraceOwnerThread = previousOwner;
		hostRetracePreviousOwnerThread = 0;
		hostReturnedAlarmOwnerThread = exitingThread;
		hostRenderHandoffRequester = 0;
		hostRenderHandoffState =
			VI_HOST_RENDER_HANDOFF_RETURN_PENDING;
		SDL_CondBroadcast(hostRenderHandoffCondition);
	} else {
		hostRetraceOwnerThread = 0;
		hostRetracePreviousOwnerThread = 0;
		hostReturnedAlarmOwnerThread = 0;
		hostRenderHandoffRequester = 0;
		hostRenderHandoffState = VI_HOST_RENDER_HANDOFF_FATAL;
		SDL_CondBroadcast(hostRenderHandoffCondition);
	}
	SDL_UnlockMutex(hostRenderHandoffMutex);
	OSRestoreInterrupts(enabled);

	__VIHostWakeRetraceWaiters();
	if (!contextReleased) {
		OSReport("libPorpoise VI: exiting render owner could not release "
		         "its context.\n");
	}
}

static void __VIHostRenderContextAlarm(
	OSAlarm* alarm,
	OSContext* context)
{
	(void)alarm;
	(void)context;
	__VIHostServiceRenderContextRequest(FALSE);
}

/* The caller holds hostRenderHandoffMutex; it remains held on return. */
static void __VIHostWaitForHandoffState(
	VIHostRenderHandoffState pendingState,
	Uint64 deadline)
{
	int waitResult = 0;

	while (hostRenderHandoffState == pendingState) {
		Uint64 now = SDL_GetTicks64();
		Uint64 remaining;

		if (now >= deadline) {
			break;
		}
		remaining = deadline - now;
		__OSHostInterruptWillWait();
		waitResult = SDL_CondWaitTimeout(
		    hostRenderHandoffCondition,
		    hostRenderHandoffMutex,
		    remaining > 0xffffffffU
		        ? 0xffffffffU
		        : (u32)remaining);
		/* Preserve the global scheduler -> handoff-mutex lock order. */
		SDL_UnlockMutex(hostRenderHandoffMutex);
		__OSHostInterruptDidWait();
		SDL_LockMutex(hostRenderHandoffMutex);
		if (waitResult != 0 && waitResult != SDL_MUTEX_TIMEDOUT) {
			break;
		}
	}
}

static BOOL __VIHostRollbackRenderContext(SDL_threadID requester)
{
	Uint64 deadline;
	BOOL enabled;

	enabled = OSDisableInterrupts();
	SDL_LockMutex(hostRenderHandoffMutex);
	if (hostRenderHandoffState == VI_HOST_RENDER_HANDOFF_IDLE &&
	    hostRetraceOwnerThread != requester) {
		/* The old owner already performed its bounded self-rollback. */
		SDL_UnlockMutex(hostRenderHandoffMutex);
		OSRestoreInterrupts(enabled);
		return TRUE;
	}
	if (hostRenderHandoffState != VI_HOST_RENDER_HANDOFF_RELEASED ||
	    hostRenderHandoffRequester != requester) {
		hostRenderHandoffState = VI_HOST_RENDER_HANDOFF_FATAL;
		hostRenderHandoffRequester = 0;
		SDL_CondBroadcast(hostRenderHandoffCondition);
		SDL_UnlockMutex(hostRenderHandoffMutex);
		OSRestoreInterrupts(enabled);
		return FALSE;
	}
	hostRenderHandoffState = VI_HOST_RENDER_HANDOFF_ROLLBACK_REQUESTED;
	/* The old owner remains inside the release service until this resolves. */
	SDL_CondBroadcast(hostRenderHandoffCondition);
	SDL_UnlockMutex(hostRenderHandoffMutex);
	OSRestoreInterrupts(enabled);

	deadline = SDL_GetTicks64() + HOST_RENDER_HANDOFF_TIMEOUT_MS;
	SDL_LockMutex(hostRenderHandoffMutex);
	__VIHostWaitForHandoffState(
	    VI_HOST_RENDER_HANDOFF_ROLLBACK_REQUESTED,
	    deadline);
	if (hostRenderHandoffState == VI_HOST_RENDER_HANDOFF_ROLLED_BACK) {
		hostRenderHandoffState = VI_HOST_RENDER_HANDOFF_IDLE;
		hostRenderHandoffRequester = 0;
		SDL_CondBroadcast(hostRenderHandoffCondition);
		SDL_UnlockMutex(hostRenderHandoffMutex);
		__VIHostWakeRetraceWaiters();
		return TRUE;
	}

	if (hostRenderHandoffState ==
	    VI_HOST_RENDER_HANDOFF_ROLLBACK_REQUESTED) {
		hostRenderHandoffState = VI_HOST_RENDER_HANDOFF_FATAL;
	}
	hostRenderHandoffRequester = 0;
	SDL_CondBroadcast(hostRenderHandoffCondition);
	SDL_UnlockMutex(hostRenderHandoffMutex);
	OSCancelAlarm(&hostRenderHandoffAlarm);
	OSReport("libPorpoise VI: could not restore the previous render "
	         "thread after a failed handoff.\n");
	return FALSE;
}

void __VIHostInitRuntime(void)
{
	BOOL enabled;

	enabled = OSDisableInterrupts();
	if (hostRetraceRuntimeInitialized) {
		OSRestoreInterrupts(enabled);
		return;
	}

	OSInitThreadQueue(&retraceQueue);
	hostRenderHandoffMutex = SDL_CreateMutex();
	hostRenderHandoffCondition = SDL_CreateCond();
	if (hostRenderHandoffMutex == NULL ||
	    hostRenderHandoffCondition == NULL) {
		if (hostRenderHandoffCondition != NULL) {
			SDL_DestroyCond(hostRenderHandoffCondition);
			hostRenderHandoffCondition = NULL;
		}
		if (hostRenderHandoffMutex != NULL) {
			SDL_DestroyMutex(hostRenderHandoffMutex);
			hostRenderHandoffMutex = NULL;
		}
		OSRestoreInterrupts(enabled);
		return;
	}

	OSCreateAlarm(&hostRenderHandoffAlarm);
	hostRetraceOwnerThread = SDL_ThreadID();
	hostRetracePreviousOwnerThread = 0;
	hostReturnedAlarmOwnerThread = 0;
	hostRenderHandoffRequester = 0;
	hostRenderHandoffState = VI_HOST_RENDER_HANDOFF_IDLE;
	__VIHostResetRetracePacing();
	if (!__OSHostRegisterThreadExitCallback(
	        __VIHostRenderOwnerThreadWillExit)) {
		SDL_DestroyCond(hostRenderHandoffCondition);
		hostRenderHandoffCondition = NULL;
		SDL_DestroyMutex(hostRenderHandoffMutex);
		hostRenderHandoffMutex = NULL;
		OSRestoreInterrupts(enabled);
		return;
	}
	__OSHostRegisterAlarmThread();
	hostRetraceRuntimeInitialized = TRUE;
	OSRestoreInterrupts(enabled);
}

BOOL __VIHostIsRenderThread(void)
{
	__VIHostInitRuntime();
	if (!hostRetraceRuntimeInitialized ||
	    hostRenderHandoffMutex == NULL) {
		return FALSE;
	}
	return __VIHostCurrentThreadOwnsReadyContext();
}

BOOL __VIHostAdoptRenderThread(void)
{
	SDL_threadID currentThread;
	SDL_threadID previousOwner;
	Uint64 deadline;
	BOOL acquired;
	BOOL enabled;
	BOOL releasedForRollback;
	BOOL transferred;

	__VIHostInitRuntime();
	if (!hostRetraceRuntimeInitialized ||
	    hostRenderHandoffMutex == NULL ||
	    hostRenderHandoffCondition == NULL) {
		return FALSE;
	}

	currentThread = SDL_ThreadID();
	if (__VIHostIsRenderThread()) {
		return __VIHostAcquireSimulatorContext();
	}
	enabled = OSDisableInterrupts();
	SDL_LockMutex(hostRenderHandoffMutex);
	if (currentThread == hostRetraceOwnerThread) {
		if (hostRenderHandoffState != VI_HOST_RENDER_HANDOFF_IDLE) {
			SDL_UnlockMutex(hostRenderHandoffMutex);
			OSRestoreInterrupts(enabled);
			OSReport("libPorpoise VI: a render-thread handoff is pending.\n");
			return FALSE;
		}
		acquired = __VIHostAcquireSimulatorContext();
		SDL_UnlockMutex(hostRenderHandoffMutex);
		OSRestoreInterrupts(enabled);
		return acquired;
	}
	if (hostRenderHandoffState != VI_HOST_RENDER_HANDOFF_IDLE) {
		SDL_UnlockMutex(hostRenderHandoffMutex);
		OSRestoreInterrupts(enabled);
		OSReport("libPorpoise VI: another render-thread handoff is pending.\n");
		return FALSE;
	}

	previousOwner = hostRetraceOwnerThread;
	hostRenderHandoffRequester = currentThread;
	hostRenderHandoffState = VI_HOST_RENDER_HANDOFF_REQUESTED;
	/*
	 * Alarm dispatch is cooperative on the old owner. Scheduling an immediate
	 * alarm both wakes an owner already in a host OS wait and is serviced by
	 * the pre-sleep alarm check if the request wins that race.
	 */
	OSSetAlarm(
	    &hostRenderHandoffAlarm,
	    0,
	    __VIHostRenderContextAlarm);
	SDL_UnlockMutex(hostRenderHandoffMutex);
	OSRestoreInterrupts(enabled);

	deadline = SDL_GetTicks64() + HOST_RENDER_HANDOFF_TIMEOUT_MS;
	SDL_LockMutex(hostRenderHandoffMutex);
	__VIHostWaitForHandoffState(
	    VI_HOST_RENDER_HANDOFF_REQUESTED,
	    deadline);

	if (hostRenderHandoffState == VI_HOST_RENDER_HANDOFF_REQUESTED) {
		/* Keep retries out until the shared handoff alarm is cancelled. */
		hostRenderHandoffState = VI_HOST_RENDER_HANDOFF_RELEASE_FAILED;
		hostRenderHandoffRequester = 0;
		SDL_CondBroadcast(hostRenderHandoffCondition);
		SDL_UnlockMutex(hostRenderHandoffMutex);
		OSCancelAlarm(&hostRenderHandoffAlarm);
		SDL_LockMutex(hostRenderHandoffMutex);
		hostRenderHandoffState = VI_HOST_RENDER_HANDOFF_IDLE;
		SDL_CondBroadcast(hostRenderHandoffCondition);
		SDL_UnlockMutex(hostRenderHandoffMutex);
		__VIHostWakeRetraceWaiters();
		OSReport("libPorpoise VI: timed out waiting for the previous render "
		         "thread to release its context.\n");
		return FALSE;
	}
	if (hostRenderHandoffState ==
	    VI_HOST_RENDER_HANDOFF_RELEASE_FAILED) {
		hostRenderHandoffState = VI_HOST_RENDER_HANDOFF_IDLE;
		hostRenderHandoffRequester = 0;
		SDL_CondBroadcast(hostRenderHandoffCondition);
		SDL_UnlockMutex(hostRenderHandoffMutex);
		__VIHostWakeRetraceWaiters();
		OSReport("libPorpoise VI: previous render thread could not release "
		         "its context.\n");
		return FALSE;
	}
	if (hostRenderHandoffState != VI_HOST_RENDER_HANDOFF_RELEASED ||
	    hostRenderHandoffRequester != currentThread) {
		hostRenderHandoffState = VI_HOST_RENDER_HANDOFF_FATAL;
		hostRenderHandoffRequester = 0;
		SDL_CondBroadcast(hostRenderHandoffCondition);
		SDL_UnlockMutex(hostRenderHandoffMutex);
		return FALSE;
	}
	SDL_UnlockMutex(hostRenderHandoffMutex);

	acquired = __VIHostAcquireSimulatorContext();
	transferred = FALSE;
	enabled = OSDisableInterrupts();
	SDL_LockMutex(hostRenderHandoffMutex);
	if (acquired &&
	    hostRenderHandoffState == VI_HOST_RENDER_HANDOFF_RELEASED &&
	    hostRenderHandoffRequester == currentThread &&
	    __OSHostTransferAlarmThread(previousOwner)) {
		hostRetraceOwnerThread = currentThread;
		hostRetracePreviousOwnerThread = previousOwner;
		hostReturnedAlarmOwnerThread = 0;
		hostCopyRetracePendingWait = FALSE;
		hostRenderHandoffState =
			VI_HOST_RENDER_HANDOFF_TRANSFERRED;
		hostRenderHandoffRequester = 0;
		SDL_CondBroadcast(hostRenderHandoffCondition);
		transferred = TRUE;
	}
	SDL_UnlockMutex(hostRenderHandoffMutex);
	OSRestoreInterrupts(enabled);

	if (transferred) {
		/*
		 * The previous owner may have serviced the release from inside a
		 * blocking OS primitive while still holding that primitive's queue
		 * mutex. Wait for its acknowledgement before this thread can enter VI.
		 */
		deadline = SDL_GetTicks64() + HOST_RENDER_HANDOFF_TIMEOUT_MS;
		SDL_LockMutex(hostRenderHandoffMutex);
		__VIHostWaitForHandoffState(
			VI_HOST_RENDER_HANDOFF_TRANSFERRED,
			deadline);
		if (hostRenderHandoffState == VI_HOST_RENDER_HANDOFF_IDLE &&
		    hostRetraceOwnerThread == currentThread) {
			SDL_UnlockMutex(hostRenderHandoffMutex);
			return TRUE;
		}
		if (hostRenderHandoffState ==
		    VI_HOST_RENDER_HANDOFF_TRANSFERRED) {
			hostRenderHandoffState = VI_HOST_RENDER_HANDOFF_FATAL;
			SDL_CondBroadcast(hostRenderHandoffCondition);
		}
		SDL_UnlockMutex(hostRenderHandoffMutex);
		OSReport("libPorpoise VI: timed out waiting for the previous "
		         "render owner to leave its release boundary.\n");
		return FALSE;
	}

	if (!acquired) {
		OSReport("libPorpoise VI: new render thread could not acquire the "
		         "released context.\n");
		releasedForRollback = TRUE;
	} else {
		OSReport("libPorpoise VI: render ownership changed before the "
		         "handoff could complete.\n");
		releasedForRollback = __VIHostReleaseSimulatorContext();
	}
	if (!releasedForRollback) {
		SDL_LockMutex(hostRenderHandoffMutex);
		hostRenderHandoffState = VI_HOST_RENDER_HANDOFF_FATAL;
		hostRenderHandoffRequester = 0;
		SDL_CondBroadcast(hostRenderHandoffCondition);
		SDL_UnlockMutex(hostRenderHandoffMutex);
		OSReport("libPorpoise VI: failed to release the new render context "
		         "for rollback.\n");
		return FALSE;
	}

	if (!__VIHostRollbackRenderContext(currentThread)) {
		return FALSE;
	}
	OSReport("libPorpoise VI: restored the previous render thread after "
	         "the handoff failed.\n");
	return FALSE;
}
#endif

/**
 * @TODO: Documentation
 */
void VIInit(void)
{
	u16 dspCfg;
	u32 value, tv;

	encoderType = getEncoderType();

	if (!(__VIRegs[VI_DISP_CONFIG] & 1)) {
		__VIInit(VI_TVMODE_NTSC_INT);
	}

#ifdef LIBPORPOISE_PORT
	SIM_VIInit();
	__VIHostInitRuntime();
	hostRetraceInProgress = FALSE;
	hostCopyRetracePendingWait = FALSE;
	hostDisplayCopyPending = FALSE;
	hostTextureCopyAwaitingDraw = FALSE;
	__VIHostResetRetracePacing();
#endif

	retraceCount = 0;
	changed      = 0;
	shdwChanged  = 0;
	changeMode   = 0;
#if defined(VERSION_GPIE01_00) || defined(VERSION_GPIJ01_01)
#else
	shdwChangeMode = 0;
#endif
	flushFlag = 0;

	__VIRegs[VI_FCT_0U] = ((((taps[0])) << 0) | (((taps[1] & ((1 << (6)) - 1))) << 10));
	__VIRegs[VI_FCT_0]  = ((((taps[1] >> 6)) << 0) | (((taps[2])) << 4));
	__VIRegs[VI_FCT_1U] = ((((taps[3])) << 0) | (((taps[4] & ((1 << (6)) - 1))) << 10));
	__VIRegs[VI_FCT_1]  = ((((taps[4] >> 6)) << 0) | (((taps[5])) << 4));
	__VIRegs[VI_FCT_2U] = ((((taps[6])) << 0) | (((taps[7] & ((1 << (6)) - 1))) << 10));
	__VIRegs[VI_FCT_2]  = ((((taps[7] >> 6)) << 0) | (((taps[8])) << 4));
	__VIRegs[VI_FCT_3U] = ((((taps[9])) << 0) | (((taps[10])) << 8));
	__VIRegs[VI_FCT_3]  = ((((taps[11])) << 0) | (((taps[12])) << 8));
	__VIRegs[VI_FCT_4U] = ((((taps[13])) << 0) | (((taps[14])) << 8));
	__VIRegs[VI_FCT_4]  = ((((taps[15])) << 0) | (((taps[16])) << 8));
	__VIRegs[VI_FCT_5U] = ((((taps[17])) << 0) | (((taps[18])) << 8));
	__VIRegs[VI_FCT_5]  = ((((taps[19])) << 0) | (((taps[20])) << 8));
	__VIRegs[VI_FCT_6U] = ((((taps[21])) << 0) | (((taps[22])) << 8));
	__VIRegs[VI_FCT_6]  = ((((taps[23])) << 0) | (((taps[24])) << 8));

	__VIRegs[VI_WIDTH] = 640;
	ImportAdjustingValues();
	HorVer.dispSizeX = 0x280U;
	HorVer.dispSizeY = 0x1E0U;
	HorVer.dispPosX  = (0x2D0 - HorVer.dispSizeX) / 2;
	HorVer.dispPosY  = (0x1E0 - HorVer.dispSizeY) / 2;
	AdjustPosition(0xF0U);
	HorVer.fbSizeX     = 0x280;
	HorVer.fbSizeY     = 0x1E0;
	HorVer.panPosX     = 0;
	HorVer.panPosY     = 0;
	HorVer.panSizeX    = 0x280;
	HorVer.panSizeY    = 0x1E0;
	HorVer.xfbMode     = 0;
	dspCfg             = __VIRegs[VI_DISP_CONFIG];
	HorVer.nonInter    = (s32)((dspCfg >> 2U) & 1);
	HorVer.tv          = (u32)((dspCfg >> 8U) & 3);
	tv                 = (HorVer.tv == 3) ? 0 : HorVer.tv;
	HorVer.timing      = getTiming((tv << 2) + HorVer.nonInter);
	regs[1]            = dspCfg;
	HorVer.wordPerLine = 0x28;
	HorVer.std         = 0x28;
	HorVer.wpl         = 0x28;
	HorVer.xof         = 0;
	HorVer.isBlack     = 1;
	HorVer.is3D        = 0;
#ifndef LIBPORPOISE_PORT
	OSInitThreadQueue(&retraceQueue);
#endif
	value = __VIRegs[VI_DISP_INT_0];
	value &= ~0x8000;
	value        = (u16)value;
	__VIRegs[VI_DISP_INT_0] = value;

	value                   = __VIRegs[VI_DISP_INT_1];
	value                   = (((u32)(value)) & ~0x00008000) | (((0)) << 15);
	__VIRegs[VI_DISP_INT_1] = value;

	PreCB  = NULL;
	PostCB = NULL;

	__OSSetInterruptHandler(__OS_INTERRUPT_PI_VI, __VIRetraceHandler);
	__OSUnmaskInterrupts(OS_INTERRUPTMASK_PI_VI);
}

/**
 * @TODO: Documentation
 */
#ifdef LIBPORPOISE_PORT
static void __VIHostAdvanceRetrace(void)
{
	hostRetraceInProgress = TRUE;
	__GXHostServiceFifoBreakpoint();
	SDL_LockMutex(retraceQueue.hostMutex);
	retraceCount++;
	SDL_UnlockMutex(retraceQueue.hostMutex);
	/*
	 * The host deliberately has no background VI/decrementer thread. Service
	 * due alarms at this deterministic retrace boundary before user callbacks.
	 */
	OSCheckAlarmQueue();

	if (PreCB != NULL) {
		PreCB(retraceCount);
	}

	if (flushFlag && VISetRegs()) {
		flushFlag = 0;
#if OS_BUILD_VERSION >= 20011217L
		SIRefreshSamplingRate();
#elif OS_BUILD_VERSION >= 20011002L
		__PADRefreshSamplingRate();
#endif
	}

	if (PostCB != NULL) {
		PostCB(retraceCount);
	}

	/*
	 * Post-retrace callbacks may issue the GXCopyDisp which completes the
	 * frame. Present only when a display copy actually supplied a new frame;
	 * retrace-only waits must not swap a cleared or stale host back buffer.
	 */
	if (hostDisplayCopyPending) {
		hostDisplayCopyPending = FALSE;
		SIM_Render();
	}
	/* Pace after presentation so CPU rendering and a driver v-sync wait count
	 * toward the same hardware retrace budget instead of being serialized with
	 * a second full-frame delay. */
	if (SIM_HostBenchmarkEnabled()) {
		const Uint64 paceFrequency = SDL_GetPerformanceFrequency();
		const Uint64 paceStart = SDL_GetPerformanceCounter();
		if (!SIM_HostBenchmarkNoPacing()) {
			__VIHostPaceRetrace();
		}
		SIM_HostBenchmarkOnRetraceEnd(
			retraceCount,
			SDL_GetPerformanceCounter() - paceStart,
			paceFrequency);
	} else {
		__VIHostPaceRetrace();
	}
	hostRetraceInProgress = FALSE;
	OSWakeupThread(&retraceQueue);
}

void __VIHostOnCopyTex(void)
{
	hostTextureCopyAwaitingDraw = TRUE;
}

void __VIHostOnDraw(void)
{
	hostTextureCopyAwaitingDraw = FALSE;
}

void __VIHostOnCopyDisp(void)
{
	__VIHostInitRuntime();
	if (!__VIHostIsRenderThread()) {
		return;
	}

	/*
	 * Render-to-texture demos sometimes follow a clearing GXCopyTex with a
	 * GXCopyDisp before issuing another draw.  The display copy is only being
	 * used to establish the next EFB clear color; presenting it would expose
	 * the intermediate render-to-texture pass.
	 */
	if (hostTextureCopyAwaitingDraw) {
		hostTextureCopyAwaitingDraw = FALSE;
		if (__GXHostApplyCopyClear != NULL) {
			__GXHostApplyCopyClear();
		}
		return;
	}

	hostDisplayCopyPending = TRUE;
	if (hostRetraceInProgress) {
		/* The active retrace presents after its post-retrace callback returns. */
		return;
	}

	__VIHostAdvanceRetrace();
	hostCopyRetracePendingWait = TRUE;
	__VIHostServiceRenderContextRequest(TRUE);
}
#endif

void VIWaitForRetrace(void)
{
	int interrupt;
	u32 startCount;

	interrupt = OSDisableInterrupts();
#ifdef LIBPORPOISE_PORT
	__VIHostInitRuntime();
	for (;;) {
		if (__VIHostIsRenderThread()) {
			if (!hostRetraceInProgress) {
				if (hostCopyRetracePendingWait) {
					hostCopyRetracePendingWait = FALSE;
				} else {
					__VIHostAdvanceRetrace();
					__VIHostServiceRenderContextRequest(TRUE);
				}
			}
			break;
		}

		/*
		 * Auxiliary emulated OS threads wait for the game/render thread to
		 * advance VI. If a failed handoff restores this thread as owner, the
		 * outer loop lets it resume driving retrace instead of sleeping on a
		 * queue that no other owner will wake.
		 */
		SDL_LockMutex(retraceQueue.hostMutex);
		startCount = retraceCount;
		while (startCount == retraceCount &&
		       !__VIHostIsRenderThread()) {
			__OSHostWaitForCondition(
			    &retraceQueue,
			    retraceQueue.hostMutex);
		}
		SDL_UnlockMutex(retraceQueue.hostMutex);
		if (startCount != retraceCount) {
			break;
		}
	}
#else
	startCount = retraceCount;
	do {
		OSSleepThread(&retraceQueue);
	} while (startCount == retraceCount);
#endif
	OSRestoreInterrupts(interrupt);
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00007C (Matching by size)
 */
static void setInterruptRegs(VITimingInfo* tm)
{
	u16 vct, hct, borrow;

	vct    = (u16)(tm->numHalfLines / 2);
	borrow = (u16)(tm->numHalfLines % 2);
	hct    = (u16)((borrow) ? tm->hlw : (u16)0);

	vct++;
	hct++;

	regs[VI_DISP_INT_0U] = (u16)hct;
	changed |= VI_BITMASK(VI_DISP_INT_0U);

	regs[VI_DISP_INT_0] = (u16)((((u32)(vct))) | (((u32)(1)) << 12) | (((u32)(0)) << 15));
	changed |= VI_BITMASK(VI_DISP_INT_0);
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000098 (Matching by size)
 */
static void setPicConfig(u16 fbSizeX, VIXFBMode xfbMode, u16 panPosX, u16 panSizeX, u8* wordPerLine, u8* std, u8* wpl, u8* xof)
{
	*wordPerLine = (u8)((fbSizeX + 15) / 16);
	*std         = (u8)((xfbMode == VI_XFBMODE_SF) ? *wordPerLine : (u8)(2 * *wordPerLine));
	*xof         = (u8)(panPosX % 16);
	*wpl         = (u8)((*xof + panSizeX + 15) / 16);

	regs[VI_HSW] = (u16)((((u32)(*std))) | (((u32)(*wpl)) << 8));
	changed |= VI_BITMASK(VI_HSW);
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 0000BC (Matching by size)
 */
static void setBBIntervalRegs(VITimingInfo* tm)
{
	u16 val;

	val                = (u16)((((u32)(tm->bs1))) | (((u32)(tm->be1)) << 5));
	regs[VI_BBI_ODD_U] = val;
	changed |= VI_BITMASK(VI_BBI_ODD_U);

	val              = (u16)((((u32)(tm->bs3))) | (((u32)(tm->be3)) << 5));
	regs[VI_BBI_ODD] = val;
	changed |= VI_BITMASK(VI_BBI_ODD);

	val                 = (u16)((((u32)(tm->bs2))) | (((u32)(tm->be2)) << 5));
	regs[VI_BBI_EVEN_U] = val;
	changed |= VI_BITMASK(VI_BBI_EVEN_U);

	val               = (u16)((((u32)(tm->bs4))) | (((u32)(tm->be4)) << 5));
	regs[VI_BBI_EVEN] = val;
	changed |= VI_BITMASK(VI_BBI_EVEN);
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00009C (Matching by size)
 */
static void setScalingRegs(u16 panSizeX, u16 dispSizeX, BOOL is3D)
{
	u32 scale;

	panSizeX = (u16)(is3D ? panSizeX * 2 : panSizeX);

	if (panSizeX < dispSizeX) {
		scale = (256 * (u32)panSizeX + (u32)dispSizeX - 1) / (u32)dispSizeX;

		regs[VI_HSR] = (u16)((((u32)(scale))) | (((u32)(1)) << 12));
		changed |= VI_BITMASK(VI_HSR);

		regs[VI_WIDTH] = (u16)((((u32)(panSizeX))));
		changed |= VI_BITMASK(VI_WIDTH);
	} else {
		regs[VI_HSR] = (u16)((((u32)(256))) | (((u32)(0)) << 12));
		changed |= VI_BITMASK(VI_HSR);
	}
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000080 (Matching by size)
 */
static void calcFbbs(u32 bufAddr, u16 panPosX, u16 panPosY, u8 wordPerLine, VIXFBMode xfbMode, u16 dispPosY, u32* tfbb, u32* bfbb)
{
	u32 bytesPerLine, xoffInWords;
	xoffInWords  = (u32)panPosX / 16;
	bytesPerLine = (u32)wordPerLine * 32;

	*tfbb = bufAddr + xoffInWords * 32 + bytesPerLine * panPosY;
	*bfbb = (xfbMode == VI_XFBMODE_SF) ? *tfbb : (*tfbb + bytesPerLine);

	if (dispPosY % 2 == 1) {
		u32 tmp = *tfbb;
		*tfbb   = *bfbb;
		*bfbb   = tmp;
	}

	*tfbb = ToPhysical(*tfbb);
	*bfbb = ToPhysical(*bfbb);
}

/**
 * @TODO: Documentation
 */
static void setFbbRegs(VIPositionInfo* hv, u32* tfbb, u32* bfbb, u32* rtfbb, u32* rbfbb)
{
	u32 shifted;
	calcFbbs(hv->bufAddr, hv->panPosX, hv->adjPanPosY, hv->wordPerLine, hv->xfbMode, hv->adjDispPosY, tfbb, bfbb);

	if (hv->is3D) {
		calcFbbs(hv->rbufAddr, hv->panPosX, hv->adjPanPosY, hv->wordPerLine, hv->xfbMode, hv->adjDispPosY, rtfbb, rbfbb);
	}

	if (IS_LOWER_16MB(*tfbb) && IS_LOWER_16MB(*bfbb) && IS_LOWER_16MB(*rtfbb) && IS_LOWER_16MB(*rbfbb)) {
		shifted = 0;
	} else {
		shifted = 1;
	}

	if (shifted) {
		*tfbb >>= 5;
		*bfbb >>= 5;
		*rtfbb >>= 5;
		*rbfbb >>= 5;
	}

	regs[VI_TOP_FIELD_BASE_LEFT_U] = (u16)(*tfbb & 0xFFFF);
	changed |= VI_BITMASK(VI_TOP_FIELD_BASE_LEFT_U);

	regs[VI_TOP_FIELD_BASE_LEFT] = (u16)((((*tfbb >> 16))) | hv->xof << 8 | shifted << 12);
	changed |= VI_BITMASK(VI_TOP_FIELD_BASE_LEFT);

	regs[VI_BTTM_FIELD_BASE_LEFT_U] = (u16)(*bfbb & 0xFFFF);
	changed |= VI_BITMASK(VI_BTTM_FIELD_BASE_LEFT_U);

	regs[VI_BTTM_FIELD_BASE_LEFT] = (u16)(*bfbb >> 16);
	changed |= VI_BITMASK(VI_BTTM_FIELD_BASE_LEFT);

	if (hv->is3D) {
		regs[VI_TOP_FIELD_BASE_RIGHT_U] = *rtfbb & 0xffff;
		changed |= VI_BITMASK(VI_TOP_FIELD_BASE_RIGHT_U);

		regs[VI_TOP_FIELD_BASE_RIGHT] = *rtfbb >> 16;
		changed |= VI_BITMASK(VI_TOP_FIELD_BASE_RIGHT);

		regs[VI_BTTM_FIELD_BASE_RIGHT_U] = *rbfbb & 0xFFFF;
		changed |= VI_BITMASK(VI_BTTM_FIELD_BASE_RIGHT_U);

		regs[VI_BTTM_FIELD_BASE_RIGHT] = *rbfbb >> 16;
		changed |= VI_BITMASK(VI_BTTM_FIELD_BASE_RIGHT);
	}
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 0000CC (Matching by size)
 */
static void setHorizontalRegs(VITimingInfo* tm, u16 dispPosX, u16 dispSizeX)
{
	u32 hbe, hbs, hbeLo, hbeHi;

	regs[VI_HORIZ_TIMING_0U] = (u16)tm->hlw;
	changed |= VI_BITMASK(VI_HORIZ_TIMING_0U);

	regs[VI_HORIZ_TIMING_0L] = (u16)(tm->hce | tm->hcs << 8);
	changed |= VI_BITMASK(VI_HORIZ_TIMING_0L);

	hbe = (u32)(tm->hbe640 - 40 + dispPosX);
	hbs = (u32)(tm->hbs640 + 40 + dispPosX - (720 - dispSizeX));

	hbeLo = hbe & ONES(9);
	hbeHi = hbe >> 9;

	regs[VI_HORIZ_TIMING_1U] = (u16)(tm->hsy | hbeLo << 7);
	changed |= VI_BITMASK(VI_HORIZ_TIMING_1U);

	regs[VI_HORIZ_TIMING_1L] = (u16)(hbeHi | hbs << 1);
	changed |= VI_BITMASK(VI_HORIZ_TIMING_1L);
}

/**
 * @TODO: Documentation
 */
static void setVerticalRegs(u16 dispPosY, u16 dispSizeY, u8 equ, u16 acv, u16 prbOdd, u16 prbEven, u16 psbOdd, u16 psbEven, BOOL black)
{
	u16 actualPrbOdd, actualPrbEven, actualPsbOdd, actualPsbEven, actualAcv, c, d;

	if (equ >= 10) {
		c = 1;
		d = 2;
	} else {
		c = 2;
		d = 1;
	}

	if (dispPosY % 2 == 0) {
		actualPrbOdd  = (u16)(prbOdd + d * dispPosY);
		actualPsbOdd  = (u16)(psbOdd + d * ((c * acv - dispSizeY) - dispPosY));
		actualPrbEven = (u16)(prbEven + d * dispPosY);
		actualPsbEven = (u16)(psbEven + d * ((c * acv - dispSizeY) - dispPosY));
	} else {
		actualPrbOdd  = (u16)(prbEven + d * dispPosY);
		actualPsbOdd  = (u16)(psbEven + d * ((c * acv - dispSizeY) - dispPosY));
		actualPrbEven = (u16)(prbOdd + d * dispPosY);
		actualPsbEven = (u16)(psbOdd + d * ((c * acv - dispSizeY) - dispPosY));
	}

	actualAcv = (u16)(dispSizeY / c);

	if (black) {
		actualPrbOdd += 2 * actualAcv - 2;
		actualPsbOdd += 2;
		actualPrbEven += 2 * actualAcv - 2;
		actualPsbEven += 2;
		actualAcv = 0;
	}

	regs[VI_VERT_TIMING] = (u16)(equ | actualAcv << 4);
	changed |= VI_BITMASK(VI_VERT_TIMING);

	regs[VI_VERT_TIMING_ODD_U] = (u16)actualPrbOdd << 0;
	changed |= VI_BITMASK(VI_VERT_TIMING_ODD_U);

	regs[VI_VERT_TIMING_ODD] = (u16)actualPsbOdd << 0;
	changed |= VI_BITMASK(VI_VERT_TIMING_ODD);

	regs[VI_VERT_TIMING_EVEN_U] = (u16)actualPrbEven << 0;
	changed |= VI_BITMASK(VI_VERT_TIMING_EVEN_U);

	regs[VI_VERT_TIMING_EVEN] = (u16)actualPsbEven << 0;
	changed |= VI_BITMASK(VI_VERT_TIMING_EVEN);
}

/**
 * @TODO: Documentation
 */
void VIConfigure(const GXRenderModeObj* obj)
{
	VITimingInfo* tm;
	u32 regDspCfg;
	BOOL enabled;
	u32 newNonInter, tvInBootrom, tvInGame;
#if OS_BUILD_VERSION >= 20011112L
	static u32 message = FALSE;
#endif

	enabled = OSDisableInterrupts();

#if defined(VERSION_GPIE01_00) || defined(VERSION_GPIJ01_01)
	if (obj->viTVmode == VI_TVMODE_NTSC_PROG) {
		HorVer.nonInter = VI_TVMODE_NTSC_PROG;
		changeMode      = 1;
	} else {
		newNonInter = (u32)obj->viTVmode & 1;
		if (HorVer.nonInter != newNonInter) {
			changeMode = 1;
		}
		HorVer.nonInter = newNonInter;
	}
#else
	newNonInter = (u32)obj->viTVmode & 3;
	if (HorVer.nonInter != newNonInter) {
		changeMode      = 1;
		HorVer.nonInter = newNonInter;
	}
#endif

	tvInBootrom = VIGetTvFormat();
	tvInGame    = (u32)obj->viTVmode >> 2;
#if 0
	tvInBootrom = *(u32*)OSPhysicalToCached(0xCC);

	if ((tvInGame == VI_NTSC) || (tvInGame == VI_MPAL)) {
	} else {
		HorVer.tv = tvInGame;
	}
#endif

#if OS_BUILD_VERSION >= 20011112L
	if (tvInGame == VI_DEBUG_PAL && message == FALSE) {
		message = TRUE;
		OSReport("***************************************\n");
		OSReport(" ! ! ! C A U T I O N ! ! !             \n");
		OSReport("This TV format \"DEBUG_PAL\" is only for \n");
		OSReport("temporary solution until PAL DAC board \n");
		OSReport("is available. Please do NOT use this   \n");
		OSReport("mode in real games!!!                  \n");
		OSReport("***************************************\n");
	}
#endif

	HorVer.tv        = tvInBootrom;
	HorVer.dispPosX  = obj->viXOrigin;
	HorVer.dispPosY  = (u16)((HorVer.nonInter == VI_NON_INTERLACE) ? (u16)(obj->viYOrigin * 2) : obj->viYOrigin);
	HorVer.dispSizeX = obj->viWidth;
	HorVer.fbSizeX   = obj->fbWidth;
	HorVer.fbSizeY   = obj->xfbHeight;
	HorVer.xfbMode   = obj->xFBmode;
	HorVer.panSizeX  = HorVer.fbSizeX;
	HorVer.panSizeY  = HorVer.fbSizeY;
	HorVer.panPosX   = 0;
	HorVer.panPosY   = 0;

	HorVer.dispSizeY = (u16)((HorVer.nonInter == VI_PROGRESSIVE) ? HorVer.panSizeY
	                         : (HorVer.xfbMode == VI_XFBMODE_SF) ? (u16)(2 * HorVer.panSizeY)
	                                                             : HorVer.panSizeY);

#if defined(VERSION_GPIE01_00) || defined(VERSION_GPIJ01_01)
	tm = getTiming(obj->viTVmode);
#else
	tm = getTiming((VITVMode)VI_TVMODE(HorVer.tv, HorVer.nonInter));
#endif
	HorVer.timing = tm;

	AdjustPosition(tm->acv);
	if (encoderType == 0) {
		HorVer.tv = VI_DEBUG;
	}
	setInterruptRegs(tm);

	regDspCfg = regs[VI_DISP_CONFIG];
	// TODO: USE BIT MACROS OR SOMETHING
	if ((HorVer.nonInter == VI_PROGRESSIVE)) {
		regDspCfg = (((u32)(regDspCfg)) & ~0x00000004) | (((u32)(1)) << 2);
	} else {
		regDspCfg = (((u32)(regDspCfg)) & ~0x00000004) | (((u32)(HorVer.nonInter & 1)) << 2);
	}

	regDspCfg = (((u32)(regDspCfg)) & ~0x00000300) | (((u32)(HorVer.tv)) << 8);

	regs[VI_DISP_CONFIG] = (u16)regDspCfg;
	changed |= VI_BITMASK(0x01);

	regDspCfg = regs[VI_CLOCK_SEL];
	if (obj->viTVmode != VI_TVMODE_NTSC_PROG) {
		regDspCfg = (u32)(regDspCfg & ~0x1);
	} else {
		regDspCfg = (u32)(regDspCfg & ~0x1) | 1;
	}

	regs[VI_CLOCK_SEL] = (u16)regDspCfg;

	changed |= 0x200;

	setScalingRegs(HorVer.panSizeX, HorVer.dispSizeX, HorVer.is3D);
	setHorizontalRegs(tm, HorVer.adjDispPosX, HorVer.dispSizeX);
	setBBIntervalRegs(tm);
	setPicConfig(HorVer.fbSizeX, HorVer.xfbMode, HorVer.panPosX, HorVer.panSizeX, &HorVer.wordPerLine, &HorVer.std, &HorVer.wpl,
	             &HorVer.xof);

	if (FBSet) {
		setFbbRegs(&HorVer, &HorVer.tfbb, &HorVer.bfbb, &HorVer.rtfbb, &HorVer.rbfbb);
	}

	setVerticalRegs(HorVer.adjDispPosY, HorVer.adjDispSizeY, tm->equ, tm->acv, tm->prbOdd, tm->prbEven, tm->psbOdd, tm->psbEven,
	                HorVer.isBlack);
	OSRestoreInterrupts(enabled);
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000384
 */
void VIConfigurePan(u16 panPosX, u16 panPosY, u16 panSizeX, u16 panSizeY)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 */
void VIFlush(void)
{
	BOOL enabled;
	s32 regIndex;
	STACK_PAD_VAR(1); // for stack.

	enabled = OSDisableInterrupts();
#if defined(VERSION_GPIE01_00) || defined(VERSION_GPIJ01_01)
#else
	shdwChangeMode |= changeMode;
	changeMode = 0;
#endif
	shdwChanged |= changed;

	while (changed) {
		regIndex           = cntlzd(changed);
		shdwRegs[regIndex] = regs[regIndex];
		changed &= ~VI_BITMASK(regIndex);
	}

	flushFlag = 1;
	OSRestoreInterrupts(enabled);
}

/**
 * @TODO: Documentation
 */
void VISetNextFrameBuffer(void* fb)
{
	BOOL enabled   = OSDisableInterrupts();
	HorVer.bufAddr = (u32)fb;
	FBSet          = 1;
	setFbbRegs(&HorVer, &HorVer.tfbb, &HorVer.bfbb, &HorVer.rtfbb, &HorVer.rbfbb);
	OSRestoreInterrupts(enabled);
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00006C
 */
void VISetNextRightFrameBuffer(void* fb)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 */
void VISetBlack(BOOL isBlack)
{
	int interrupt;
	VITimingInfo* tm;

	interrupt      = OSDisableInterrupts();
	HorVer.isBlack = isBlack;
	tm             = HorVer.timing;
	setVerticalRegs(HorVer.adjDispPosY, HorVer.dispSizeY, tm->equ, tm->acv, tm->prbOdd, tm->prbEven, tm->psbOdd, tm->psbEven,
	                HorVer.isBlack);
	OSRestoreInterrupts(interrupt);
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000100
 */
void VISet3D(void)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 */
u32 VIGetRetraceCount(void)
{
	return retraceCount;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000050 (`OS_BUILD_VERSION >= 20011002L`) (Matching by size)
 * @note UNUSED Size: 000058 (`OS_BUILD_VERSION <  20011002L`) (Matching by size)
 */
static u32 getCurrentHalfLine(void)
{
#ifdef LIBPORPOISE_PORT
	VITimingInfo* tm = CurrTiming != NULL ? CurrTiming : HorVer.timing;
	u32 numHalfLines = tm != NULL ? tm->numHalfLines : 525;

	/*
	 * The host has no continuously ticking VI beam counters. Model the two
	 * interlaced fields at retrace granularity instead of reading the static
	 * emulated hardware registers and underflowing their zero values.
	 */
	return (retraceCount & 1) != 0 ? numHalfLines : 0;
#else
	u32 hcount;
	u32 vcount0;
	u32 vcount;

#if OS_BUILD_VERSION >= 20011002L
#else
	VITimingInfo* tm;
	tm = HorVer.timing;
#endif

	vcount = __VIRegs[VI_VERT_COUNT] & 0x7FF;
	do {
		vcount0 = vcount;
		hcount  = __VIRegs[VI_HORIZ_COUNT] & 0x7FF;
		vcount  = __VIRegs[VI_VERT_COUNT] & 0x7FF;
	} while (vcount0 != vcount);

#if OS_BUILD_VERSION >= 20011002L
	return ((vcount - 1) * 2) + ((hcount - 1) / CurrTiming->hlw);
#else
	return ((vcount - 1) * 2) + ((hcount - 1) / tm->hlw);
#endif
#endif
}

/**
 * @TODO: Documentation
 */
static u32 getCurrentFieldEvenOdd()
{
	u16 value;
	u32 nin;
	u32 fmt;
	VITVMode tvMode;
	u32 nhlines;
	VITimingInfo* tm;

#if OS_BUILD_VERSION >= 20011002L
	#ifdef LIBPORPOISE_PORT
	if ((retraceCount & 1) == 0) {
	#else
	if (getCurrentHalfLine() < CurrTiming->numHalfLines) {
	#endif
		return 1;
	}
#else
	if (__VIRegs[VI_CLOCK_SEL] & 1) {
		tm = getTiming(VI_TVMODE_NTSC_PROG);
	} else {
		value  = __VIRegs[VI_DISP_CONFIG];
		nin    = ((value >> 2U) & 1);
		fmt    = ((value >> 8U) & 3);
		tvMode = (fmt << 2) + nin;
		tm     = getTiming(tvMode);
	}
	nhlines = tm->numHalfLines;
	if (getCurrentHalfLine() < nhlines) {
		return 1;
	}
#endif
	return 0;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 0000A8 (OS_BUILD_VERSION >= 20011002L) (Matching by size)
 * @note UNUSED Size: 0000F4                                 (Matching by size)
 */
u32 VIGetNextField(void)
{
	s32 nextField;
	BOOL enabled;

	enabled   = OSDisableInterrupts();
	nextField = getCurrentFieldEvenOdd() ^ 1;
	OSRestoreInterrupts(enabled);
	return nextField ^ (HorVer.panPosY & 1);
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000098 (OS_BUILD_VERSION >= 20011002L) (Matching by size)
 * @note UNUSED Size: 0000A4                                 (Matching by size)
 */
u32 VIGetCurrentLine(void)
{
	u32 halfLine;
	VITimingInfo* tm;
	BOOL enabled;

#if OS_BUILD_VERSION >= 20011002L
	#ifdef LIBPORPOISE_PORT
	tm = CurrTiming != NULL ? CurrTiming : HorVer.timing;
	if (tm == NULL) {
		return 0;
	}
	#else
	tm = CurrTiming;
	#endif
#else
	// I am making up this version difference.  It might be something else.
	tm = HorVer.timing;
#endif
	enabled  = OSDisableInterrupts();
	halfLine = getCurrentHalfLine();
	OSRestoreInterrupts(enabled);
	if (halfLine >= tm->numHalfLines) {
		halfLine -= tm->numHalfLines;
	}
	return halfLine >> 1U;
}

/**
 * @TODO: Documentation
 */
u32 VIGetTvFormat(void)
{
#if OS_BUILD_VERSION >= 20011112L
	s32 enabled;
	s32 format;

	enabled = OSDisableInterrupts();
	switch (CurrTvMode) {
	case 3:
	case 0:
	{
		format = 0;
		break;
	}
	case 4:
	case 1:
	{
		format = 1;
		break;
	}
	case 5:
	case 2:
	{
		format = CurrTvMode;
		break;
	}
	}
	OSRestoreInterrupts(enabled);

	// We have this assert from some other decomp, but originally it was placed before the variable was
	// initialized?  I moved it down to here for now, but I'm just guessing at where it really belongs.
	OSAssertMsgLine(0x80D, format == 0 || format == 1 || format == 2,
	                "VIGetTvFormat(): Wrong format is stored in lo mem. Maybe lo mem is trashed");

	return format;
#else
	return *(u32*)OSPhysicalToCached(0xCC);
#endif
}

/**
 * @TODO: Documentation
 */
u32 VIGetDTVStatus(void)
{
	u32 stat;
	int interrupt;

	interrupt = OSDisableInterrupts();
	stat      = (__VIRegs[VI_DTV_STAT] & 3);
	OSRestoreInterrupts(interrupt);
	return (stat & 1);
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 0002C8
 */
void __VISetAdjustingValues(void)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00004C
 */
void __VIGetAdjustingValues(void)
{
	TRAP_UNIMPLEMENTED;
}
