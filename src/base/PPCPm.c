/*---------------------------------------------------------------------------*
  PPCPm.c - Performance Monitor API for libPorpoise
  
  PC-compatible stubs for performance monitoring functions.
  On PC, these functions are stubs that return dummy values.
 *---------------------------------------------------------------------------*/

#include <dolphin/types.h>
#include <dolphin/base/PPCArch.h>
#include <dolphin/base/PPCPm.h>

void PMBegin(void) {
    /* On PC, performance monitoring is not implemented */
    /* Set registers to hold mode for compatibility */
    PPCMtmmcr0(MMCR0_PMC1_HOLD | MMCR0_PMC2_HOLD);
    PPCMtmmcr1(MMCR1_PMC3_HOLD | MMCR1_PMC4_HOLD);
    PPCMtpmc1(0);
    PPCMtpmc2(0);
    PPCMtpmc3(0);
    PPCMtpmc4(0);
    /* Set up event selection (though they won't actually count on PC) */
    PPCMtmmcr0(MMCR0_PMC1_CYCLE | MMCR0_PMC2_IC_FETCH_MISS);
    PPCMtmmcr1(MMCR1_PMC3_L1_MISS_CYCLE | MMCR1_PMC4_INSTRUCTION);
}

void PMEnd(void) {
    /* Stop performance monitoring */
    PPCMtmmcr0(MMCR0_PMC1_HOLD | MMCR0_PMC2_HOLD);
    PPCMtmmcr1(MMCR1_PMC3_HOLD | MMCR1_PMC4_HOLD);
}

u32 PMCycles(void) {
    return PPCMfpmc1();
}

u32 PML1FetchMisses(void) {
    return PPCMfpmc2();
}

u32 PML1MissCycles(void) {
    return PPCMfpmc3();
}

u32 PMInstructions(void) {
    return PPCMfpmc4();
}

