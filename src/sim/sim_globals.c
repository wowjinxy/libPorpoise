#include <dolphin.h>

vu32 __AIRegs[8];
vu32 __DIRegs[16];
vu16 __DSPRegs[32];
vu32 __EXIRegs[16];
vu16 __MEMRegs[64];
vu32 __PIRegs[13];
vu32 __SIRegs[64];
vu16 __VIRegs[59];

u8 __ArenaHi[2];
u8 __ArenaLo[2];

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
u32 __OSBusClock;
u32 __OSCoreClock;
u32 OS_UNK_CODE;
u32 OS_HOT_RESET_CODE;
u16 __OSWirelessPadFixMode;
u8 GameChoice;

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