#include <dolphin.h>

#ifdef LIBPORPOISE_BUILD_LINUX
#include <signal.h>
#elif defined(LIBPORPOISE_BUILD_WIN64)
#include <intrin.h>
#endif

vu32 __AIRegs[8];
vu16 __CPRegs[51];
vu32 __DIRegs[16];
vu16 __DSPRegs[32];
vu32 __EXIRegs[16];
vu16 __MEMRegs[64];
vu16 __PERegs[24];
vu32 __PIRegs[13];
vu32 __SIRegs[64];
vu16 __VIRegs[59];

s32 __EXIProbeStartTime[2];

void* OS_BOOT_REGION_START;
void* OS_BOOT_REGION_END;
u32 OS_RESET_CODE;
volatile u8 OS_REBOOT_BOOL;
struct OSModuleQueue __OSModuleInfoList;
const void* __OSStringTable;
volatile OSContext* __OSCurrentContext;
volatile OSContext* __OSFPUContext;
OSThreadQueue __OSActiveThreadQueue;
OSThread* __OSCurrentThread;
volatile OSInterruptMask __OSPriorInterruptMask;
volatile OSInterruptMask __OSCurrentInterruptMask;
u32 __OSBusClock = 1000000;
u32 __OSCoreClock = 1000000;
u32 OS_UNK_CODE;
u32 OS_HOT_RESET_CODE;
u16 __OSWirelessPadFixMode;
u8 GameChoice;
volatile int __OSTVMode;

volatile PPCWGPipe GXWGFifo;

// TODO: Move these to a stubs file or something like that
void __OSEVStart(void) {}
void __OSEVEnd(void) {}
void __OSEVSetNumber(void) {}
void __OSExceptionVector(void) {}

void __DBVECTOR(void) {}
void __OSDBINTSTART(void) {}
void __OSDBINTEND(void) {}
void __OSDBJUMPSTART(void) {}
void __OSDBJUMPEND(void) {}
void __OSSystemCallVectorStart() {}
void __OSSystemCallVectorEnd() {}


// Gamecube main memory space
// 24MB
u8 __ArenaLo[2];
u8 s_SIM_main_mem_buf[24 * 1024 * 1024];
u8 __ArenaHi[2];

void SIM_DebugBreak(void)
{
#ifdef LIBPORPOISE_BUILD_LINUX
	raise(SIGTRAP);
#elif defined(LIBPORPOISE_BUILD_WIN64)
	__debugbreak();
#else
	OSReport("Warning: SIM_DebugBreak called but it is not supported on this platform!\n");
#endif
}
