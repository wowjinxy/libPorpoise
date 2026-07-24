#ifndef __DEMOSTATS_H__
#define __DEMOSTATS_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DEMO_STAT_TL,
    DEMO_STAT_BL,
    DEMO_STAT_TLD,
    DEMO_STAT_BLD,
    DEMO_STAT_IO
} DEMOStatDispMode;

typedef enum
{
    DEMO_STAT_GP0,
    DEMO_STAT_GP1,
    DEMO_STAT_MEM,
    DEMO_STAT_PIX,
    DEMO_STAT_VC,
    DEMO_STAT_FR,
    DEMO_STAT_TBW,
    DEMO_STAT_TBP,
    DEMO_STAT_MYC,
    DEMO_STAT_MYR
} DEMOStatType;

typedef enum
{
    DEMO_STAT_MEM_CP,
    DEMO_STAT_MEM_TC,
    DEMO_STAT_MEM_CPUR,
    DEMO_STAT_MEM_CPUW,
    DEMO_STAT_MEM_DSP,
    DEMO_STAT_MEM_IO,
    DEMO_STAT_MEM_VI,
    DEMO_STAT_MEM_PE,
    DEMO_STAT_MEM_RF,
    DEMO_STAT_MEM_FI
} DEMOMemStatArg;

typedef enum
{
    DEMO_STAT_PIX_TI,
    DEMO_STAT_PIX_TO,
    DEMO_STAT_PIX_BI,
    DEMO_STAT_PIX_BO,
    DEMO_STAT_PIX_CI,
    DEMO_STAT_PIX_CC
} DEMOPixStatArg;

typedef enum
{
    DEMO_STAT_VC_CHK,
    DEMO_STAT_VC_MISS,
    DEMO_STAT_VC_STALL
} DEMOVcStatArg;

#ifdef  __MWERKS__
#pragma warn_padding off
#endif

typedef struct
{
    char            text[50];
    DEMOStatType    stat_type;
    u32             stat;
    u32             count;
} DEMOStatObj;

#ifdef  __MWERKS__
#pragma warn_padding reset
#endif

extern GXBool DemoStatEnable;

extern void DEMOSetStats(DEMOStatObj* stat, u32 nstats, DEMOStatDispMode disp);

#ifdef __cplusplus
}
#endif

#endif

