#include <demo.h>
#include <dolphin.h>
#include <string.h>

#define FLIPPER_CLOCK 162.0F

#define DEMO_TEXT_TOP 16
#define DEMO_TEXT_BOT 16
#define DEMO_TEXT_LFT 16
#define DEMO_TEXT_RHT 16

#define DEMO_CHAR_WD 8
#define DEMO_CHAR_HT 8
#define DEMO_CHAR_YSP 2

GXBool DemoStatEnable = GX_FALSE;

static DEMOStatObj *DemoStat = NULL;
static u32 DemoStatIndx = 0;
static u32 DemoStatMaxIndx = 0;
static u32 DemoStatClocks = 0;

static u32 DemoStatDisp = 0;
static u32 DemoStatStrLen = 0;

static u32 topPixIn, topPixOut;
static u32 botPixIn, botPixOut;
static u32 clrPixIn, copyClks;

static u32 vcCheck, vcMiss, vcStall;

static u32 cpReq, tcReq, cpuRdReq, cpuWrReq, dspReq, ioReq, viReq, peReq, rfReq,
    fiReq;

extern void DEMOPrintStats(void);
extern void DEMOUpdateStats(GXBool inc);

static void DEMOWriteStats(GXBool update);

void DEMOSetStats(DEMOStatObj *stat, u32 nstats, DEMOStatDispMode disp) {
  if (stat == NULL || nstats == 0) {
    DemoStatEnable = GX_DISABLE;
  } else {
    DemoStatEnable = GX_TRUE;
    DemoStat = stat;
    DemoStatIndx = 0;
    DemoStatMaxIndx = nstats;
    DemoStatDisp = disp;
    DemoStatStrLen = strlen(DemoStat[0].text);
  }
}

static void DEMOWriteStats(GXBool update) {
  u32 cnt0, cnt1, cnt2, cnt3, cnt4;
  u32 cnt5, cnt6, cnt7, cnt8, cnt9;

  switch (DemoStat[DemoStatIndx].stat_type) {
  case DEMO_STAT_GP0:
    if (update) {
      cnt0 = GXReadGP0Metric();
      DemoStat[DemoStatIndx].count = cnt0;
      GXSetGP0Metric(GX_PERF0_NONE);
    } else {
      GXSetGP0Metric((GXPerf0)DemoStat[DemoStatIndx].stat);
      GXClearGP0Metric();
    }

    break;

  case DEMO_STAT_GP1:
    if (update) {
      cnt0 = GXReadGP1Metric();
      DemoStat[DemoStatIndx].count = cnt0;
      GXSetGP1Metric(GX_PERF1_NONE);
    } else {
      GXSetGP1Metric((GXPerf1)DemoStat[DemoStatIndx].stat);
      GXClearGP1Metric();
    }
    break;

  case DEMO_STAT_MEM:
    if (update) {
      GXReadMemMetric(&cnt0, &cnt1, &cnt2, &cnt3, &cnt4, &cnt5, &cnt6, &cnt7,
                      &cnt8, &cnt9);
      cpReq = cnt0;
      tcReq = cnt1;
      cpuRdReq = cnt2;
      cpuWrReq = cnt3;
      dspReq = cnt4;
      ioReq = cnt5;
      viReq = cnt6;
      peReq = cnt7;
      rfReq = cnt8;
      fiReq = cnt9;
    } else {
      GXClearMemMetric();
    }
    break;

  case DEMO_STAT_PIX:
    if (update) {
      GXReadPixMetric(&cnt0, &cnt1, &cnt2, &cnt3, &cnt4, &cnt5);
      topPixIn = cnt0;
      topPixOut = cnt1;
      botPixIn = cnt2;
      botPixOut = cnt3;
      clrPixIn = cnt4;
      copyClks = cnt5;
    } else {
      GXClearPixMetric();
    }
    break;

  case DEMO_STAT_VC:
    if (update) {
      GXReadVCacheMetric(&cnt0, &cnt1, &cnt2);
      vcCheck = cnt0;
      vcMiss = cnt1;
      vcStall = cnt2;
    } else {
      GXSetVCacheMetric(GX_VC_POS);
      GXClearVCacheMetric();
    }
    break;

  case DEMO_STAT_FR:
    if (update) {
      GXReadPixMetric(&cnt0, &cnt1, &cnt2, &cnt3, &cnt4, &cnt5);
      topPixIn = cnt0;
      topPixOut = cnt1;
      botPixIn = cnt2;
      botPixOut = cnt3;
      clrPixIn = cnt4;
      copyClks = cnt5;
      DemoStatClocks = GXReadGP0Metric();
      GXSetGP0Metric(GX_PERF0_NONE);
    } else {
      GXClearPixMetric();
      GXSetGP0Metric(GX_PERF0_CLOCKS);
      GXClearGP0Metric();
    }
    break;

  case DEMO_STAT_TBP:
  case DEMO_STAT_TBW:
    GXClearPixMetric();
    if (update) {
      GXReadPixMetric(&cnt0, &cnt1, &cnt2, &cnt3, &cnt4, &cnt5);
      topPixIn = cnt0;
      topPixOut = cnt1;
      botPixIn = cnt2;
      botPixOut = cnt3;
      clrPixIn = cnt4;
      copyClks = cnt5;
      DemoStatClocks = GXReadGP0Metric();
      GXReadMemMetric(&cnt0, &cnt1, &cnt2, &cnt3, &cnt4, &cnt5, &cnt6, &cnt7,
                      &cnt8, &cnt9);
      tcReq = cnt1;
      GXSetGP0Metric(GX_PERF0_NONE);
    } else {
      GXClearMemMetric();
      GXSetGP0Metric(GX_PERF0_CLOCKS);
      GXClearGP0Metric();
    }
    break;

  case DEMO_STAT_MYC:
  case DEMO_STAT_MYR:

    break;
  default:
    OSHalt("DEMOSetStats: Unknown demo stat type\n");
    break;
  }
}

void DEMOUpdateStats(GXBool inc) {
  DEMOWriteStats(inc);

  if (inc) {
    DemoStatIndx++;
    if (DemoStatIndx == DemoStatMaxIndx) {
      DemoStatIndx = 0;
    }
  }
}

#define DEMOStatPrintf(str, val)                                               \
  DEMOPrintf(text_x, text_y, 0, str, DemoStat[i].text, val)

#define DEMOStatOSReport(str, val) OSReport(str, DemoStat[i].text, val)

void DEMOPrintStats(void) {
  GXRenderModeObj *rmode;
  u32 i;
  s16 text_x, text_y, text_yinc;
  u16 wd, ht;
  f32 rate;

  if (DemoStatDisp == DEMO_STAT_IO) {
    for (i = 0; i < DemoStatMaxIndx; i++) {
      switch (DemoStat[i].stat_type) {
      case DEMO_STAT_PIX:
        switch (DemoStat[i].stat) {
        case DEMO_STAT_PIX_TI:
          DEMOStatOSReport("%s: %8d\n", topPixIn);
          break;
        case DEMO_STAT_PIX_TO:
          DEMOStatOSReport("%s: %8d\n", topPixOut);
          break;
        case DEMO_STAT_PIX_BI:
          DEMOStatOSReport("%s: %8d\n", botPixIn);
          break;
        case DEMO_STAT_PIX_BO:
          DEMOStatOSReport("%s: %8d\n", botPixOut);
          break;
        case DEMO_STAT_PIX_CI:
          DEMOStatOSReport("%s: %8d\n", clrPixIn);
          break;
        case DEMO_STAT_PIX_CC:
          DEMOStatOSReport("%s: %8d\n", copyClks);
          break;
        }
        break;

      case DEMO_STAT_FR:
        rate = FLIPPER_CLOCK * (f32)(topPixIn + botPixIn) /
               (f32)(DemoStatClocks - copyClks);
        DEMOStatOSReport("%s: %8.2f\n", rate);
        break;

      case DEMO_STAT_TBW:
        rate = FLIPPER_CLOCK * (f32)(tcReq * 32) /
               (f32)(DemoStatClocks - copyClks);
        DEMOStatOSReport("%s: %8.2f\n", rate);
        break;

      case DEMO_STAT_TBP:
        rate = (f32)(tcReq * 32) / (topPixIn + botPixIn);
        DEMOStatOSReport("%s: %8.2f\n", rate);
        break;

      case DEMO_STAT_VC:
        switch (DemoStat[i].stat) {
        case DEMO_STAT_VC_CHK:
          DEMOStatOSReport("%s: %8d\n", vcCheck);
          break;
        case DEMO_STAT_VC_MISS:
          DEMOStatOSReport("%s: %8d\n", vcMiss);
          break;
        case DEMO_STAT_VC_STALL:
          DEMOStatOSReport("%s: %8d\n", vcStall);
          break;
        }
        break;

      case DEMO_STAT_MYR:
        rate = (f32)DemoStat[i].stat / (f32)DemoStat[i].count;
        DEMOStatOSReport("%s: %8.2f\n", rate);
        break;

      case DEMO_STAT_MEM:
        switch (DemoStat[i].stat) {
        case DEMO_STAT_MEM_CP:
          DEMOStatOSReport("%s: %8d\n", cpReq);
          break;
        case DEMO_STAT_MEM_TC:
          DEMOStatOSReport("%s: %8d\n", tcReq);
          break;
        case DEMO_STAT_MEM_CPUR:
          DEMOStatOSReport("%s: %8d\n", cpuRdReq);
          break;
        case DEMO_STAT_MEM_CPUW:
          DEMOStatOSReport("%s: %8d\n", cpuWrReq);
          break;
        case DEMO_STAT_MEM_DSP:
          DEMOStatOSReport("%s: %8d\n", dspReq);
          break;
        case DEMO_STAT_MEM_IO:
          DEMOStatOSReport("%s: %8d\n", ioReq);
          break;
        case DEMO_STAT_MEM_VI:
          DEMOStatOSReport("%s: %8d\n", viReq);
          break;
        case DEMO_STAT_MEM_PE:
          DEMOStatOSReport("%s: %8d\n", peReq);
          break;
        case DEMO_STAT_MEM_RF:
          DEMOStatOSReport("%s: %8d\n", rfReq);
          break;
        case DEMO_STAT_MEM_FI:
          DEMOStatOSReport("%s: %8d\n", fiReq);
          break;
        }
        break;

      default:
        DEMOStatOSReport("%s: %8d\n", DemoStat[i].count);
        break;
      }
    }
  } else {
    rmode = DEMOGetRenderModeObj();

    switch (DemoStatDisp) {
    case DEMO_STAT_TL:

      text_x = DEMO_TEXT_LFT;
      text_y = DEMO_TEXT_TOP;
      text_yinc = DEMO_CHAR_HT + DEMO_CHAR_YSP;
      wd = rmode->fbWidth;
      ht = rmode->xfbHeight;
      break;
    case DEMO_STAT_BL:

      text_x = DEMO_TEXT_LFT;
      text_y = (s16)(rmode->xfbHeight - DEMO_TEXT_BOT - DEMO_CHAR_HT);
      text_yinc = -(DEMO_CHAR_HT + DEMO_CHAR_YSP);
      wd = rmode->fbWidth;
      ht = rmode->xfbHeight;
      break;
    case DEMO_STAT_TLD:

      text_x = (s16)(DEMO_TEXT_LFT / 2);
      text_y = (s16)(DEMO_TEXT_TOP / 2);
      text_yinc = DEMO_CHAR_HT + DEMO_CHAR_YSP / 2;
      wd = (u16)(rmode->fbWidth / 2);
      ht = (u16)(rmode->xfbHeight / 2);
      break;
    case DEMO_STAT_BLD:

      text_x = (s16)(DEMO_TEXT_LFT / 2);
      text_y = (s16)((rmode->xfbHeight - DEMO_TEXT_BOT - DEMO_CHAR_HT) / 2);
      text_yinc = -(DEMO_CHAR_HT + DEMO_CHAR_YSP / 2);
      wd = (u16)(rmode->fbWidth / 2);
      ht = (u16)(rmode->xfbHeight / 2);
      break;
    }

    DEMOInitCaption(DM_FT_OPQ, wd, ht);

    for (i = 0; i < DemoStatMaxIndx; i++) {
      switch (DemoStat[i].stat_type) {
      case DEMO_STAT_PIX:

        switch (DemoStat[i].stat) {
        case DEMO_STAT_PIX_TI:
          DEMOStatPrintf("%s: %8d\n", topPixIn);
          break;
        case DEMO_STAT_PIX_TO:
          DEMOStatPrintf("%s: %8d\n", topPixOut);
          break;
        case DEMO_STAT_PIX_BI:
          DEMOStatPrintf("%s: %8d\n", botPixIn);
          break;
        case DEMO_STAT_PIX_BO:
          DEMOStatPrintf("%s: %8d\n", botPixOut);
          break;
        case DEMO_STAT_PIX_CI:
          DEMOStatPrintf("%s: %8d\n", clrPixIn);
          break;
        case DEMO_STAT_PIX_CC:
          DEMOStatPrintf("%s: %8d\n", copyClks);
        }
        break;

      case DEMO_STAT_FR:

        rate = FLIPPER_CLOCK * (f32)(topPixIn + botPixIn) /
               (f32)(DemoStatClocks - copyClks);
        DEMOStatPrintf("%s: %8.2f\n", rate);
        break;

      case DEMO_STAT_TBW:

        rate = FLIPPER_CLOCK * (f32)(tcReq * 32) /
               (f32)(DemoStatClocks - copyClks);
        DEMOStatPrintf("%s: %8.2f\n", rate);
        break;

      case DEMO_STAT_TBP:

        rate = (f32)(tcReq * 32) / (f32)(topPixIn - botPixIn);
        DEMOStatPrintf("%s: %8.3f\n", rate);
        break;

      case DEMO_STAT_VC:

        switch (DemoStat[i].stat) {
        case DEMO_STAT_VC_CHK:
          DEMOStatPrintf("%s: %8d\n", vcCheck);
          break;
        case DEMO_STAT_VC_MISS:
          DEMOStatPrintf("%s: %8d\n", vcMiss);
          break;
        case DEMO_STAT_VC_STALL:
          DEMOStatPrintf("%s: %8d\n", vcStall);
        }
        break;

      case DEMO_STAT_MEM:

        switch (DemoStat[i].stat) {
        case DEMO_STAT_MEM_CP:
          DEMOStatPrintf("%s: %8d\n", cpReq);
          break;
        case DEMO_STAT_MEM_TC:
          DEMOStatPrintf("%s: %8d\n", tcReq);
          break;
        case DEMO_STAT_MEM_CPUR:
          DEMOStatPrintf("%s: %8d\n", cpuRdReq);
          break;
        case DEMO_STAT_MEM_CPUW:
          DEMOStatPrintf("%s: %8d\n", cpuWrReq);
          break;
        case DEMO_STAT_MEM_DSP:
          DEMOStatPrintf("%s: %8d\n", dspReq);
          break;
        case DEMO_STAT_MEM_IO:
          DEMOStatPrintf("%s: %8d\n", ioReq);
          break;
        case DEMO_STAT_MEM_VI:
          DEMOStatPrintf("%s: %8d\n", viReq);
          break;
        case DEMO_STAT_MEM_PE:
          DEMOStatPrintf("%s: %8d\n", peReq);
          break;
        case DEMO_STAT_MEM_RF:
          DEMOStatPrintf("%s: %8d\n", rfReq);
          break;
        case DEMO_STAT_MEM_FI:
          DEMOStatPrintf("%s: %8d\n", fiReq);
          break;
        }
        break;

      case DEMO_STAT_GP0:
      case DEMO_STAT_GP1:
      case DEMO_STAT_MYC:
        DEMOStatPrintf("%s: %8d", DemoStat[i].count);
        break;

      case DEMO_STAT_MYR:
        rate = (f32)DemoStat[i].stat / (f32)DemoStat[i].count;
        DEMOStatPrintf("%s: %8.3f", rate);
        break;

      default:
        OSReport("Undefined stat type %d in DEMOPrintStats()\n",
                 DemoStat[i].stat_type);
        break;
      }

      text_y += text_yinc;
    }
  }
}
