/*---------------------------------------------------------------------------*
  PPCArch.h - PowerPC Architecture Definitions for libPorpoise

  PC-compatible version of the base library's PowerPC architecture definitions.
  Provides register definitions and accessor function declarations.

  On PC: Most functions are stubs that return dummy values or are no-ops.
  This provides API compatibility for games that use these functions.
 *---------------------------------------------------------------------------*/

#ifndef DOLPHIN_BASE_PPCARCH_H
#define DOLPHIN_BASE_PPCARCH_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

    /*---------------------------------------------------------------------------*
        SPR (Special Purpose Register) Definitions
     *---------------------------------------------------------------------------*/

#define CTR     9
#define XER     1
#define LR      8

#define UPMC1   937
#define UPMC2   938
#define UPMC3   941
#define UPMC4   942

#define USIA    939

#define UMMCR0  936
#define UMMCR1  940

#define HID0    1008
#define HID1    1009

#define PVR     287

#define IBAT0U  528
#define IBAT0L  529
#define IBAT1U  530
#define IBAT1L  531
#define IBAT2U  532
#define IBAT2L  533
#define IBAT3U  534
#define IBAT3L  535

#define DBAT0U  536
#define DBAT0L  537
#define DBAT1U  538
#define DBAT1L  539
#define DBAT2U  540
#define DBAT2L  541
#define DBAT3U  542
#define DBAT3L  543

#define SDR1    25

#define SPRG0   272
#define SPRG1   273
#define SPRG2   274
#define SPRG3   275

#define DAR     19
#define DSISR   18

#define SRR0    26
#define SRR1    27

#define EAR     282

#define DABR    1013

#define TBL     284
#define TBU     285

#define L2CR    1017

#define DEC     22

#define IABR    1010

#define PMC1    953
#define PMC2    954
#define PMC3    957
#define PMC4    958

#define SIA     955

#define MMCR0   952
#define MMCR1   956

#define THRM1   1020
#define THRM2   1021
#define THRM3   1022

#define ICTC    1019

     /* GEKKO Registers */
#define GQR0    912
#define GQR1    913
#define GQR2    914
#define GQR3    915
#define GQR4    916
#define GQR5    917
#define GQR6    918
#define GQR7    919

#define HID2    920

#define WPAR    921

#define DMA_U   922
#define DMA_L   923

/* Broadway additional Registers */
#define IBAT4U  560
#define IBAT4L  561
#define IBAT5U  562
#define IBAT5L  563
#define IBAT6U  564
#define IBAT6L  565
#define IBAT7U  566
#define IBAT7L  567

#define DBAT4U  568
#define DBAT4L  569
#define DBAT5U  570
#define DBAT5L  571
#define DBAT6U  572
#define DBAT6L  573
#define DBAT7U  574
#define DBAT7L  575

/*---------------------------------------------------------------------------*
    MSR bits
 *---------------------------------------------------------------------------*/
#define MSR_POW         0x00040000
#define MSR_ILE         0x00010000
#define MSR_EE          0x00008000
#define MSR_PR          0x00004000
#define MSR_FP          0x00002000
#define MSR_ME          0x00001000
#define MSR_FE0         0x00000800
#define MSR_SE          0x00000400
#define MSR_BE          0x00000200
#define MSR_FE1         0x00000100
#define MSR_IP          0x00000040
#define MSR_IR          0x00000020
#define MSR_DR          0x00000010
#define MSR_PM          0x00000004
#define MSR_RI          0x00000002
#define MSR_LE          0x00000001

 /*---------------------------------------------------------------------------*
     HID0 bits
  *---------------------------------------------------------------------------*/
#define HID0_ICE        0x00008000
#define HID0_DCE        0x00004000
#define HID0_ILOCK      0x00002000
#define HID0_DLOCK      0x00001000
#define HID0_ICFI       0x00000800
#define HID0_DCFI       0x00000400
#define HID0_SPD        0x00000200
#define HID0_ABE        0x00000008

  /*---------------------------------------------------------------------------*
      HID2 bits (GEKKO)
   *---------------------------------------------------------------------------*/
#define HID2_LSQE           0x80000000
#define HID2_WPE            0x40000000
#define HID2_PSE            0x20000000
#define HID2_LCE            0x10000000

   /*---------------------------------------------------------------------------*
       L2CR bits
    *---------------------------------------------------------------------------*/
#define L2CR_L2E                    0x80000000
#define L2CR_L2CE                   0x40000000
#define L2CR_L2DO                   0x00400000
#define L2CR_L2I                    0x00200000
#define L2CR_L2WT                   0x00080000
#define L2CR_L2TS                   0x00040000
#define L2CR_L2IP                   0x00000001

    /*---------------------------------------------------------------------------*
        MMCR0 bits
     *---------------------------------------------------------------------------*/
#define MMCR0_DIS                   0x80000000
#define MMCR0_DP                    0x40000000
#define MMCR0_DU                    0x20000000
#define MMCR0_DMS                   0x10000000
#define MMCR0_DMR                   0x08000000
#define MMCR0_ENINT                 0x04000000
#define MMCR0_DISCOUNT              0x02000000
#define MMCR0_RTCSELECT_MASK        0x01800000
#define MMCR0_RTCSELECT_63          0x00000000
#define MMCR0_RTCSELECT_55          0x00800000
#define MMCR0_RTCSELECT_51          0x01000000
#define MMCR0_RTCSELECT_47          0x01800000
#define MMCR0_INTONBITTRANS         0x00400000
#define MMCR0_THRESHOLD_MASK        0x003F0000
#define MMCR0_THRESHOLD(n)          ((n) << 16)
#define MMCR0_PMC1INTCONTROL        0x00008000
#define MMCR0_PMC2INTCONTROL        0x00004000
#define MMCR0_PMCTRIGGER            0x00002000
#define MMCR0_PMC1SELECT_MASK       0x00001FC0
#define MMCR0_PMC2SELECT_MASK       0x0000003F

     /*---------------------------------------------------------------------------*
         MMCR1 bits
      *---------------------------------------------------------------------------*/
#define MMCR1_PMC3SELECT_MASK       0xF8000000
#define MMCR1_PMC4SELECT_MASK       0x07C00000

      /*---------------------------------------------------------------------------*
          PMCn bits
       *---------------------------------------------------------------------------*/
#define PMC1_OV                     0x80000000
#define PMC1_COUNTER                0x7FFFFFFF
#define PMC2_OV                     0x80000000
#define PMC2_COUNTER                0x7FFFFFFF
#define PMC3_OV                     0x80000000
#define PMC3_COUNTER                0x7FFFFFFF
#define PMC4_OV                     0x80000000
#define PMC4_COUNTER                0x7FFFFFFF

       /*---------------------------------------------------------------------------*
           PMC1 Events
        *---------------------------------------------------------------------------*/
#define MMCR0_PMC1_HOLD             0x00000000
#define MMCR0_PMC1_CYCLE            0x00000040
#define MMCR0_PMC1_INSTRUCTION      0x00000080
#define MMCR0_PMC1_TRANSITION       0x000000C0
#define MMCR0_PMC1_DISPATCHED       0x00000100
#define MMCR0_PMC1_EIEIO            0x00000140
#define MMCR0_PMC1_ITLB_CYCLE       0x00000180
#define MMCR0_PMC1_L2_HIT           0x000001C0
#define MMCR0_PMC1_EA               0x00000200
#define MMCR0_PMC1_IABR             0x00000240
#define MMCR0_PMC1_L1_MISS          0x00000280
#define MMCR0_PMC1_Bx_UNRESOLVED    0x000002C0
#define MMCR0_PMC1_Bx_STALL_CYCLE   0x00000300

        /*---------------------------------------------------------------------------*
            PMC2 Events
         *---------------------------------------------------------------------------*/
#define MMCR0_PMC2_HOLD             0x00000000
#define MMCR0_PMC2_CYCLE            0x00000001
#define MMCR0_PMC2_INSTRUCTION      0x00000002
#define MMCR0_PMC2_TRANSITION       0x00000003
#define MMCR0_PMC2_DISPATCHED       0x00000004
#define MMCR0_PMC2_IC_MISS          0x00000005
#define MMCR0_PMC2_ITLB_MISS        0x00000006
#define MMCR0_PMC2_L2_I_MISS        0x00000007
#define MMCR0_PMC2_Bx_FALL_TROUGH   0x00000008
#define MMCR0_PMC2_RESERVED_LOAD    0x0000000A
#define MMCR0_PMC2_LOAD_STORE       0x0000000B
#define MMCR0_PMC2_SNOOP            0x0000000C
#define MMCR0_PMC2_L1_CASTOUT       0x0000000D
#define MMCR0_PMC2_SYSTEM           0x0000000E
#define MMCR0_PMC2_IC_FETCH_MISS    0x0000000F
#define MMCR0_PMC2_Bx_OUT_OF_ORDER  0x00000010

         /*---------------------------------------------------------------------------*
             PMC3 Events
          *---------------------------------------------------------------------------*/
#define MMCR1_PMC3_HOLD             0x00000000
#define MMCR1_PMC3_CYCLE            0x08000000
#define MMCR1_PMC3_INSTRUCTION      0x10000000
#define MMCR1_PMC3_TRANSITION       0x18000000
#define MMCR1_PMC3_DISPATCHED       0x20000000
#define MMCR1_PMC3_DC_MISS          0x28000000
#define MMCR1_PMC3_DTLB_MISS        0x30000000
#define MMCR1_PMC3_L2_D_MISS        0x38000000
#define MMCR1_PMC3_Bx_TAKEN         0x40000000
#define MMCR1_PMC3_COND_STORE       0x50000000
#define MMCR1_PMC3_FPU              0x58000000
#define MMCR1_PMC3_L2_SNOOP_CASTOUT 0x60000000
#define MMCR1_PMC3_L2_HIT           0x68000000
#define MMCR1_PMC3_L1_MISS_CYCLE    0x78000000
#define MMCR1_PMC3_Bx_SECOND        0x80000000
#define MMCR1_PMC3_BPU_LR_CR        0x88000000

          /*---------------------------------------------------------------------------*
              PMC4 Events
           *---------------------------------------------------------------------------*/
#define MMCR1_PMC4_HOLD             0x00000000
#define MMCR1_PMC4_CYCLE            0x00400000
#define MMCR1_PMC4_INSTRUCTION      0x00800000
#define MMCR1_PMC4_TRANSITION       0x00C00000
#define MMCR1_PMC4_DISPATCHED       0x01000000
#define MMCR1_PMC4_L2_CASTOUT       0x01400000
#define MMCR1_PMC4_DTLB_CYCLE       0x01800000
#define MMCR1_PMC4_Bx_MISSED        0x02000000
#define MMCR1_PMC4_COND_STORE_INT   0x02800000
#define MMCR1_PMC4_SYNC             0x02C00000
#define MMCR1_PMC4_SNOOP_RETRY      0x03000000
#define MMCR1_PMC4_INTEGER          0x03400000
#define MMCR1_PMC4_BPU_THIRD        0x03800000

           /*---------------------------------------------------------------------------*
               FPSCR bits
            *---------------------------------------------------------------------------*/
#ifndef FPSCR_FX
#define FPSCR_FX            0x80000000
#define FPSCR_FEX           0x40000000
#define FPSCR_VX            0x20000000
#define FPSCR_OX            0x10000000
#define FPSCR_UX            0x08000000
#define FPSCR_ZX            0x04000000
#define FPSCR_XX            0x02000000
#define FPSCR_VXSNAN        0x01000000
#define FPSCR_VXISI         0x00800000
#define FPSCR_VXIDI         0x00400000
#define FPSCR_VXZDZ         0x00200000
#define FPSCR_VXIMZ         0x00100000
#define FPSCR_VXVC          0x00080000
#define FPSCR_FR            0x00040000
#define FPSCR_FI            0x00020000
#define FPSCR_VXSOFT        0x00000400
#define FPSCR_VXSQRT        0x00000200
#define FPSCR_VXCVI         0x00000100
#define FPSCR_VE            0x00000080
#define FPSCR_OE            0x00000040
#define FPSCR_UE            0x00000020
#define FPSCR_ZE            0x00000010
#define FPSCR_XE            0x00000008
#define FPSCR_NI            0x00000004
#endif

            /*---------------------------------------------------------------------------*
                Function Declarations
             *---------------------------------------------------------------------------*/

             /* MSR functions */
    u32  PPCMfmsr(void);
    void PPCMtmsr(u32 newMSR);
    u32  PPCOrMsr(u32 value);
    u32  PPCAndMsr(u32 value);
    u32  PPCAndCMsr(u32 value);

    /* HID0 functions */
    u32  PPCMfhid0(void);
    void PPCMthid0(u32 newHID0);

    /* HID1 functions */
    u32  PPCMfhid1(void);

    /* HID2 functions (GEKKO) */
    u32  PPCMfhid2(void);
    void PPCMthid2(u32 newhid2);

    /* WPAR functions (GEKKO) */
    u32  PPCMfwpar(void);
    void PPCMtwpar(u32 newwpar);

    /* DMA functions (GEKKO) */
    u32  PPCMfdmaU(void);
    u32  PPCMfdmaL(void);
    void PPCMtdmaU(u32 newdmau);
    void PPCMtdmaL(u32 newdmal);

    /* L2CR functions */
    u32  PPCMfl2cr(void);
    void PPCMtl2cr(u32 newL2cr);

    /* DEC functions */
    void PPCMtdec(u32 newDec);
    u32  PPCMfdec(void);

    /* Sync functions */
    void PPCSync(void);
    void PPCEieio(void);

    /* Halt function */
    void PPCHalt(void);

    /* Performance Monitor functions */
    u32  PPCMfmmcr0(void);
    void PPCMtmmcr0(u32 newMmcr0);
    u32  PPCMfmmcr1(void);
    void PPCMtmmcr1(u32 newMmcr1);
    u32  PPCMfpmc1(void);
    void PPCMtpmc1(u32 newPmc1);
    u32  PPCMfpmc2(void);
    void PPCMtpmc2(u32 newPmc2);
    u32  PPCMfpmc3(void);
    void PPCMtpmc3(u32 newPmc3);
    u32  PPCMfpmc4(void);
    void PPCMtpmc4(u32 newPmc4);
    u32  PPCMfsia(void);
    void PPCMtsia(u32 newSia);

    /* PVR function */
    u32  PPCMfpvr(void);

    /* FPSCR functions */
    u32  PPCMffpscr(void);
    void PPCMtfpscr(u32 newFPSCR);

    /* Mode functions */
    void PPCEnableSpeculation(void);
    void PPCDisableSpeculation(void);
    void PPCSetFpIEEEMode(void);
    void PPCSetFpNonIEEEMode(void);

    /* Broadway HID4 functions */
    u32  PPCMfhid4(void);
    void PPCMthid4(u32 newhid4);

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_BASE_PPCARCH_H */

