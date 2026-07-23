/*---------------------------------------------------------------------------*
  Project:  libPorpoise Demo Library
  File:     DEMOStats.h
  
  Statistics display for demo framework.
  
  Based on Nintendo's Revolution SDK demo library.
 *---------------------------------------------------------------------------*/

#ifndef DOLPHIN_DEMO_STATS_H
#define DOLPHIN_DEMO_STATS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dolphin/types.h>
#include <dolphin/gx/GXEnum.h>

/*---------------------------------------------------------------------------*
    DEMOStats.c
 *---------------------------------------------------------------------------*/
// statistic data display style
typedef enum
{
    DEMO_STAT_TL,       // top left
    DEMO_STAT_BL,       // bottom left
    DEMO_STAT_TLD,      // double-size font, top-left
    DEMO_STAT_BLD,      // double-size font, bottom-left
    DEMO_STAT_IO        // dump using OSReport
} DEMOStatDispMode;

// statistic types
typedef enum
{
    DEMO_STAT_GP0,      // GXReadGP0Metric
    DEMO_STAT_GP1,      // GXReadGP1Metric
    DEMO_STAT_MEM,      // GXReadMemMetric
    DEMO_STAT_PIX,      // GXReadPixMetric
    DEMO_STAT_VC,       // GXReadVCacheMetric
    DEMO_STAT_FR,       // Fill rate calc
    DEMO_STAT_TBW,      // Texture bandwidth calc
    DEMO_STAT_TBP,      // Texture B/pixel
    DEMO_STAT_MYC,      // print out user-computed count
    DEMO_STAT_MYR       // print out user-computed rate (stat/count)
} DEMOStatType;

// used as "stat" argument when stat_type == DEMO_STAT_MEM
typedef enum
{
    DEMO_STAT_MEM_CP,   // GXReadMemMetric(CP Req.)
    DEMO_STAT_MEM_TC,   // GXReadMemMetric(TC Req.)
    DEMO_STAT_MEM_CPUR, // GXReadMemMetric(CPU Rd Req.)
    DEMO_STAT_MEM_CPUW, // GXReadMemMetric(CPU Wr Req.)
    DEMO_STAT_MEM_DSP,  // GXReadMemMetric(DSP Req.)
    DEMO_STAT_MEM_IO,   // GXReadMemMetric(IO Req.)
    DEMO_STAT_MEM_VI,   // GXReadMemMetric(VI Req.)
    DEMO_STAT_MEM_PE,   // GXReadMemMetric(PE Req.)
    DEMO_STAT_MEM_RF,   // GXReadMemMetric(RF Req.)
    DEMO_STAT_MEM_FI    // GXReadMemMetric(FI Req.)
} DEMOMemStatArg;

// used as "stat" argument when stat_type == DEMO_STAT_PIX
typedef enum
{
    DEMO_STAT_PIX_TI,   // GXReadPixMetric(Top Pixel In)
    DEMO_STAT_PIX_TO,   // GXReadPixMetric(Top Pixel Out)
    DEMO_STAT_PIX_BI,   // GXReadPixMetric(Bottom Pixel In)
    DEMO_STAT_PIX_BO,   // GXReadPixMetric(Bottom Pixel Out)
    DEMO_STAT_PIX_CI,   // GXReadPixMetric(Color Pixel In)
    DEMO_STAT_PIX_CC    // GXReadPixMetric(Copy Clocks)
} DEMOPixStatArg;

// used as "stat" argument when stat_type == DEMO_STAT_VC
typedef enum
{
    DEMO_STAT_VC_CHK,   // GXReadVCacheMetric(Check)
    DEMO_STAT_VC_MISS,  // GXReadVCacheMetric(Miss)
    DEMO_STAT_VC_STALL  // GXReadVCacheMetric(Stall)
} DEMOVcStatArg;

typedef struct
{
    char            text[50];  // 8 x 50 = 400 pixels
    DEMOStatType    stat_type; // statics type
    u32             stat;      // metric to measure
    u32             count;     // count returned from metric function
} DEMOStatObj;

// Global variables
extern GXBool  DemoStatEnable;

// Function Prototype
extern void DEMOSetStats (
    DEMOStatObj*        stat,
    u32                 nstats,
    DEMOStatDispMode    disp );

/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_DEMO_STATS_H */

