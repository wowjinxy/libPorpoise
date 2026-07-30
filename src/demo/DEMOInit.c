#include <demo.h>
#include <dolphin.h>

extern void DEMOUpdateStats(GXBool inc);
extern void DEMOPrintStats(void);

void *DemoFrameBuffer1;
void *DemoFrameBuffer2;
void *DemoCurrentBuffer;

static GXBool DemoFirstFrame = GX_TRUE;

#define DEFAULT_FIFO_SIZE (256 * 1024)

static void *DefaultFifo;
static GXFifoObj *DefaultFifoObj;

static GXRenderModeObj *rmode;
static GXRenderModeObj rmodeobj;

static u32 allocatedFrameBufferSize = 0;

static BOOL GPHangWorkaround = FALSE;

MEMAllocator DemoAllocator1;
MEMAllocator DemoAllocator2;

static vu32 FrameCount;
static u32 FrameMissThreshold;

#define DEMO_START_FRAME_TOKEN 0xFEEB
#define DEMO_END_FRAME_TOKEN 0xB00B

typedef struct {
  void *start;
  void *end;
} meminfo;

static void __DEMOInitRenderMode(GXRenderModeObj *mode);
static void __DEMOInitMem();
static void __DEMOInitGX();
static void __DEMOInitVI();

static void __NoHangRetraceCallback(u32 count);
static void __NoHangDoneRender();

static void __DEMODiagnoseHang();

void DEMOInit(GXRenderModeObj *mode) {
  OSInit();
  DVDInit();
  VIInit();
  DEMOPadInit();
  __DEMOInitRenderMode(mode);
  __DEMOInitMem();
  VIConfigure(rmode);

  DefaultFifo = OSAlloc(DEFAULT_FIFO_SIZE);
  DefaultFifoObj = GXInit(DefaultFifo, DEFAULT_FIFO_SIZE);

  __DEMOInitGX();
  __DEMOInitVI();
}

static void __DEMOInitRenderMode(GXRenderModeObj *mode) {

  if (mode != NULL) {
    rmodeobj = *mode;
    rmode = &rmodeobj;
  } else {
    switch (VIGetTvFormat()) {
    case VI_NTSC:
      rmode = &GXNtsc480IntDf;
      break;
    case VI_PAL:
      rmode = &GXPal528IntDf;
      break;
    case VI_EURGB60:
      rmode = &GXEurgb60Hz480IntDf;
      break;
    case VI_MPAL:
      rmode = &GXMpal480IntDf;
      break;
    default:
      OSHalt("DEMOInit: invalid TV format\n");
      break;
    }


    GXAdjustForOverscan(rmode, &rmodeobj, 0, 16);

    rmode = &rmodeobj;
  }
}


static void __DEMOInitMem(void) {
  void *arenaLo;
  void *arenaHi;
  u32 fbSize;

  arenaLo = OSGetArenaLo();
  arenaHi = OSGetArenaHi();

  


  fbSize = VIPadFrameBufferWidth(rmode->fbWidth) * rmode->xfbHeight *
           (u32)VI_DISPLAY_PIX_SZ;
  allocatedFrameBufferSize = fbSize;
  #ifdef LIBPORPOISE_PORT
  DemoFrameBuffer1 = (void *)OSRoundUp32B((u64)arenaLo);
  DemoFrameBuffer2 = (void *)OSRoundUp32B((u64)DemoFrameBuffer1 + fbSize);
  #else
  DemoFrameBuffer1 = (void *)OSRoundUp32B((u32)arenaLo);
  DemoFrameBuffer2 = (void *)OSRoundUp32B((u32)DemoFrameBuffer1 + fbSize);
  #endif
  DemoCurrentBuffer = DemoFrameBuffer2;

  #ifdef LIBPORPOISE_PORT
  arenaLo = (void *)OSRoundUp32B((u64)DemoFrameBuffer2 + fbSize);
  #else
  arenaLo = (void *)OSRoundUp32B((u32)DemoFrameBuffer2 + fbSize);
  #endif
  OSSetArenaLo(arenaLo);

  


  arenaLo = OSGetArenaLo();
  arenaHi = OSGetArenaHi();
  arenaLo = OSInitAlloc(arenaLo, arenaHi, 1);
  OSSetArenaLo(arenaLo);


  arenaLo = (void *)OSRoundUp32B(arenaLo);
  arenaHi = (void *)OSRoundDown32B(arenaHi);


  OSSetCurrentHeap(OSCreateHeap(arenaLo, arenaHi));
  MEMInitAllocatorForOSHeap(&DemoAllocator1, __OSCurrHeap);
  MEMInitAllocatorForOSHeap(&DemoAllocator2, __OSCurrHeap);

  OSSetArenaLo(arenaLo = arenaHi);
}


static void __DEMOInitGX(void) {
  u16 xfbHeight;
  f32 yScale;

  



  GXSetViewport(0.0F, 0.0F, (f32)rmode->fbWidth, (f32)rmode->efbHeight, 0.0F,
                1.0F);
  GXSetScissor(0, 0, (u32)rmode->fbWidth, (u32)rmode->efbHeight);

  yScale = GXGetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
  xfbHeight = (u16)GXSetDispCopyYScale(yScale);
  GXSetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
  GXSetDispCopyDst(rmode->fbWidth, xfbHeight);
  GXSetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);

  if (rmode->aa)
    GXSetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
  else
    GXSetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

  GXCopyDisp(DemoCurrentBuffer, GX_TRUE);

  GXSetDispCopyGamma(GX_GM_1_0);
}


static void __DEMOInitVI(void) {
  u32 nin;


  VISetNextFrameBuffer(DemoFrameBuffer1);
  DemoCurrentBuffer = DemoFrameBuffer2;


  VIFlush();


  VIWaitForRetrace();


  nin = (u32)rmode->viTVmode & 1;
  if (nin)
    VIWaitForRetrace();
}




void DEMOBeforeRender(void) {

#ifndef EMU
  if (GPHangWorkaround) {

    GXSetDrawSync(DEMO_START_FRAME_TOKEN);


    GXClearGPMetric();
  }
#endif


  if (rmode->field_rendering) {
    GXSetViewportJitter(0.0F, 0.0F, (float)rmode->fbWidth,
                        (float)rmode->efbHeight, 0.0F, 1.0F, VIGetNextField());
  } else {
    GXSetViewport(0.0F, 0.0F, (float)rmode->fbWidth, (float)rmode->efbHeight,
                  0.0F, 1.0F);
  }


  GXInvalidateVtxCache();

#ifndef EMU

  GXInvalidateTexAll();
#endif
}


void DEMODoneRender(void) {

  if (GPHangWorkaround) {
    ASSERTMSG(!DemoStatEnable,
              "DEMOStats and GP hang diagnosis are mutually exclusive");
    __NoHangDoneRender();
    return;
  }


  if (DemoStatEnable) {
    GXDrawDone();
    DEMOUpdateStats(GX_TRUE);
    DEMOPrintStats();
    GXDrawDone();
    DEMOUpdateStats(GX_FALSE);
  }


  GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
  GXSetColorUpdate(GX_TRUE);


  GXCopyDisp(DemoCurrentBuffer, GX_TRUE);


  GXDrawDone();


  DEMOSwapBuffers();
}


void DEMOSwapBuffers(void) {

  VISetNextFrameBuffer(DemoCurrentBuffer);


  if (DemoFirstFrame) {
    VISetBlack(FALSE);
    DemoFirstFrame = GX_FALSE;
  }


  VIFlush();


  VIWaitForRetrace();


  if (DemoCurrentBuffer == DemoFrameBuffer1)
    DemoCurrentBuffer = DemoFrameBuffer2;
  else
    DemoCurrentBuffer = DemoFrameBuffer1;
}


#if (GX_REV > 1)
void DEMOSetTevColorIn(GXTevStageID stage, GXTevColorArg a, GXTevColorArg b,
                       GXTevColorArg c, GXTevColorArg d) {
  u32 swap = 0;

  if (a == GX_CC_TEXC) {
    swap = GX_CC_TEXRRR - 1;
  } else if (a >= GX_CC_TEXRRR) {
    swap = a;
    a = GX_CC_TEXC;
  }

  if (b == GX_CC_TEXC) {
    swap = GX_CC_TEXRRR - 1;
  } else if (b >= GX_CC_TEXRRR) {
    swap = b;
    b = GX_CC_TEXC;
  }

  if (c == GX_CC_TEXC) {
    swap = GX_CC_TEXRRR - 1;
  } else if (c >= GX_CC_TEXRRR) {
    swap = c;
    c = GX_CC_TEXC;
  }

  if (d == GX_CC_TEXC) {
    swap = GX_CC_TEXRRR - 1;
  } else if (d >= GX_CC_TEXRRR) {
    swap = d;
    d = GX_CC_TEXC;
  }

  GXSetTevColorIn(stage, a, b, c, d);

  if (swap > 0)
    GXSetTevSwapMode(stage, GX_TEV_SWAP0,
                     (GXTevSwapSel)(swap - GX_CC_TEXRRR + 1));
}
#endif


#if (GX_REV > 1)
void DEMOSetTevOp(GXTevStageID id, GXTevMode mode) {
  GXTevColorArg carg = GX_CC_RASC;
  GXTevAlphaArg aarg = GX_CA_RASA;

  if (id != GX_TEVSTAGE0) {
    carg = GX_CC_CPREV;
    aarg = GX_CA_APREV;
  }

  switch (mode) {
  case GX_MODULATE:
    DEMOSetTevColorIn(id, GX_CC_ZERO, GX_CC_TEXC, carg, GX_CC_ZERO);
    GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_TEXA, aarg, GX_CA_ZERO);
    break;
  case GX_DECAL:
    DEMOSetTevColorIn(id, carg, GX_CC_TEXC, GX_CC_TEXA, GX_CC_ZERO);
    GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, aarg);
    break;
  case GX_BLEND:
    DEMOSetTevColorIn(id, carg, GX_CC_ONE, GX_CC_TEXC, GX_CC_ZERO);
    GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_TEXA, aarg, GX_CA_ZERO);
    break;
  case GX_REPLACE:
    DEMOSetTevColorIn(id, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
    GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
    break;
  case GX_PASSCLR:
    DEMOSetTevColorIn(id, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, carg);
    GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, aarg);
    break;
  default:
    ASSERTMSG(0, "DEMOSetTevOp: Invalid Tev Mode");
    break;
  }

  GXSetTevColorOp(id, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV);
  GXSetTevAlphaOp(id, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV);
}
#endif


GXRenderModeObj *DEMOGetRenderModeObj(void) { return rmode; }


void *DEMOGetCurrentBuffer(void) { return DemoCurrentBuffer; }



void DEMOEnableGPHangWorkaround(u32 timeoutFrames) {
#ifdef EMU
#pragma unused(timeoutFrames)
#else
  if (timeoutFrames) {
    ASSERTMSG(!DemoStatEnable,
              "DEMOStats and GP hang diagnosis are mutually exclusive");
    GPHangWorkaround = TRUE;
    FrameMissThreshold = timeoutFrames;
    VISetPreRetraceCallback(__NoHangRetraceCallback);


    DEMOSetGPHangMetric(GX_TRUE);
  } else {
    GPHangWorkaround = FALSE;
    FrameMissThreshold = 0;
    DEMOSetGPHangMetric(GX_FALSE);
    VISetPreRetraceCallback(NULL);
  }
#endif
}


static void __NoHangRetraceCallback(u32 count) {
#pragma unused(count)
  static u32 ovFrameCount = 0;
  static u32 lastOvc = 0;
  u32 ovc;
  GXBool overhi, junk;



  FrameCount++;



  GXGetGPStatus(&overhi, &junk, &junk, &junk, &junk);
  ovc = GXGetOverflowCount();

  if (overhi && (ovc == lastOvc)) {
    ovFrameCount++;
    if (ovFrameCount >= FrameMissThreshold) {

      OSReport("---------WARNING : HANG AT HIGH WATERMARK----------\n");

      __DEMODiagnoseHang();


      OSHalt("Halting program");
    }
  } else {
    lastOvc = ovc;
    ovFrameCount = 0;
  }
}


static void __NoHangDoneRender() {
  BOOL abort = FALSE;
  GXCopyDisp(DemoCurrentBuffer, GX_TRUE);
  GXSetDrawSync(DEMO_END_FRAME_TOKEN);

  FrameCount = 0;

  while ((GXReadDrawSync() != DEMO_END_FRAME_TOKEN) && !abort) {
    if (FrameCount >= FrameMissThreshold) {
      OSReport("---------WARNING : ABORTING FRAME----------\n");
      abort = TRUE;
      __DEMODiagnoseHang();
      DEMOReInit(rmode);

      DEMOSetGPHangMetric(GX_TRUE);
    }
  }

  DEMOSwapBuffers();
}


void DEMOSetGPHangMetric(GXBool enable) {
#ifdef EMU
#pragma unused(enable)
#else
  if (enable) {

    GXSetGPMetric(GX_PERF0_NONE, GX_PERF1_NONE);


    GXCmd1u8(GX_LOAD_BP_REG);
    GXParam1u32(0x2402c004);


    GXCmd1u8(GX_LOAD_BP_REG);
    GXParam1u32(0x23000020);


    GXCmd1u8(GX_LOAD_XF_REG);
    GXParam1u16(0x0000);
    GXParam1u16(0x1006);
    GXParam1u32(0x00084400);
  } else {

    GXCmd1u8(GX_LOAD_BP_REG);
    GXParam1u32(0x24000000);


    GXCmd1u8(GX_LOAD_BP_REG);
    GXParam1u32(0x23000000);


    GXCmd1u8(GX_LOAD_XF_REG);
    GXParam1u16(0x0000);
    GXParam1u16(0x1006);
    GXParam1u32(0x00000000);
  }
#endif
}


static void __DEMODiagnoseHang() {
  u32 xfTop0, xfBot0, suRdy0, r0Rdy0;
  u32 xfTop1, xfBot1, suRdy1, r0Rdy1;
  u32 xfTopD, xfBotD, suRdyD, r0RdyD;
  GXBool readIdle, cmdIdle, junk;


  GXReadXfRasMetric(&xfBot0, &xfTop0, &r0Rdy0, &suRdy0);
  GXReadXfRasMetric(&xfBot1, &xfTop1, &r0Rdy1, &suRdy1);


  xfTopD = (xfTop1 - xfTop0) == 0;
  xfBotD = (xfBot1 - xfBot0) == 0;
  suRdyD = (suRdy1 - suRdy0) > 0;
  r0RdyD = (r0Rdy1 - r0Rdy0) > 0;


  GXGetGPStatus(&junk, &junk, &readIdle, &cmdIdle, &junk);

  OSReport("GP status %d%d%d%d%d%d --> ", readIdle, cmdIdle, xfTopD, xfBotD,
           suRdyD, r0RdyD);


  if (!xfBotD && suRdyD) {
    OSReport("GP hang due to XF stall bug.\n");
  } else if (!xfTopD && xfBotD && suRdyD) {
    OSReport("GP hang due to unterminated primitive.\n");
  } else if (!cmdIdle && xfTopD && xfBotD && suRdyD) {
    OSReport("GP hang due to illegal instruction.\n");
  } else if (readIdle && cmdIdle && xfTopD && xfBotD && suRdyD && r0RdyD) {
    OSReport("GP appears to be not hung (waiting for input).\n");
  } else {
    OSReport("GP is in unknown state.\n");
  }
}


void DEMOReInit(GXRenderModeObj *mode) {
  u32 fbSize;


  GXFifoObj tmpobj;
  void *tmpFifo = OSAlloc(64 * 1024);


  GXFifoObj *realFifoObj = GXGetCPUFifo();
  void *realFifoBase = GXGetFifoBase(realFifoObj);
  u32 realFifoSize = GXGetFifoSize(realFifoObj);


  GXAbortFrame();

  GXInitFifoBase(&tmpobj, tmpFifo, 64 * 1024);

  GXSetCPUFifo(&tmpobj);
  GXSetGPFifo(&tmpobj);

  
  __DEMOInitRenderMode(mode);


  fbSize = VIPadFrameBufferWidth(rmode->fbWidth) * rmode->xfbHeight *
           (u32)VI_DISPLAY_PIX_SZ;
  ASSERTMSG(fbSize <= allocatedFrameBufferSize,
            "DEMOReInit - Previously "
            "allocated frame buffer is too small for the new render mode.");


  DefaultFifoObj = GXInit(realFifoBase, realFifoSize);

  __DEMOInitGX();


  VIConfigure(rmode);
  __DEMOInitVI();


  OSFree(tmpFifo);
}


