#include <demo.h>

extern "C" void main();

#define STRUT_LN 130
#define STRUT_SD 4
#define JOINT_SD 10

s16 Verts_s16[] ATTRIBUTE_ALIGN(32) = {
    -STRUT_SD, STRUT_SD,  -STRUT_SD, STRUT_SD,  STRUT_SD,  -STRUT_SD, STRUT_SD,
    STRUT_SD,  STRUT_SD,  -STRUT_SD, STRUT_SD,  STRUT_SD,  STRUT_SD,  -STRUT_SD,
    -STRUT_SD, STRUT_SD,  -STRUT_SD, STRUT_SD,  STRUT_SD,  STRUT_LN,  -STRUT_SD,
    STRUT_SD,  STRUT_LN,  STRUT_SD,  -STRUT_SD, STRUT_LN,  STRUT_SD,  -STRUT_SD,
    STRUT_SD,  -STRUT_LN, STRUT_SD,  STRUT_SD,  -STRUT_LN, STRUT_SD,  -STRUT_SD,
    -STRUT_LN, STRUT_LN,  STRUT_SD,  -STRUT_SD, STRUT_LN,  STRUT_SD,  STRUT_SD,
    STRUT_LN,  -STRUT_SD, STRUT_SD,  -JOINT_SD, JOINT_SD,  -JOINT_SD, JOINT_SD,
    JOINT_SD,  -JOINT_SD, JOINT_SD,  JOINT_SD,  JOINT_SD,  -JOINT_SD, JOINT_SD,
    JOINT_SD,  JOINT_SD,  -JOINT_SD, -JOINT_SD, JOINT_SD,  -JOINT_SD, JOINT_SD,
    -JOINT_SD, -JOINT_SD, JOINT_SD};

u8 Colors_rgba8[] ATTRIBUTE_ALIGN(32) = {42, 42, 50, 255, 80, 80, 80, 255, 114, 114, 110, 255};

void main(void);
static void CameraInit(Mtx v);
static void DrawInit(void);
static void DrawTick(Mtx v);
static void AnimTick(Mtx v);
static void PrintIntro(void);

void main(void) {
  Mtx v;
  PADStatus pad[PAD_MAX_CONTROLLERS];

  pad[0].button = 0;

  DEMOInit(NULL);

  CameraInit(v);
  DrawInit();

  PrintIntro();

  while (!(pad[0].button & PAD_BUTTON_MENU)) {
    DEMOBeforeRender();
    DrawTick(v);
    DEMODoneRender();
    AnimTick(v);
    PADRead(pad);
  }

  OSHalt("End of demo");
}

static void CameraInit(Mtx v) {
  Mtx44 p;
  Vec up = {0.20F, 0.97F, 0.0F};
  Vec camLoc = {90.0F, 110.0F, 13.0F};
  Vec objPt = {-110.0F, -70.0F, -190.0F};
  f32 left = 24.0F;
  f32 top = 32.0F;
  f32 znear = 50.0F;
  f32 zfar = 2000.0F;

  MTXFrustum(p, left, -left, -top, top, znear, zfar);
  GXSetProjection(p, GX_PERSPECTIVE);

  MTXLookAt(v, &camLoc, &up, &objPt);
}

static void DrawInit(void) {
  GXColor black = {0, 0, 0, 0};

  GXSetCopyClear(black, 0x00FFFFFF);

  GXClearVtxDesc();
  GXSetVtxDesc(GX_VA_POS, GX_INDEX8);
  GXSetVtxDesc(GX_VA_CLR0, GX_INDEX8);

  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);

  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

  GXSetArray(GX_VA_POS, Verts_s16, 3 * sizeof(s16));

  GXSetArray(GX_VA_CLR0, Colors_rgba8, 4 * sizeof(u8));

  GXSetNumChans(1);
  GXSetNumTexGens(0);
  GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
  GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
}

static inline void Vertex(u8 v, u8 c) {
  GXPosition1x8(v);
  GXColor1x8(c);
}

static inline void DrawFsQuad(u8 v0, u8 v1, u8 v2, u8 v3, u8 c) {
  Vertex(v0, c);
  Vertex(v1, c);
  Vertex(v2, c);
  Vertex(v3, c);
}

static void DrawTick(Mtx v) {
  f32 x;
  f32 y;
  f32 z;
  Mtx m;
  Mtx mv;

  GXSetNumTexGens(0);
  GXSetNumTevStages(1);
  GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);

  MTXIdentity(m);

  for (x = -10 * STRUT_LN; x < 2 * STRUT_LN; x += STRUT_LN) {
    for (y = -10 * STRUT_LN; y < STRUT_LN; y += STRUT_LN) {
      for (z = STRUT_LN; z > -10 * STRUT_LN; z -= STRUT_LN) {
        MTXRowCol(m, 0, 3) = x;
        MTXRowCol(m, 1, 3) = y;
        MTXRowCol(m, 2, 3) = z;
        MTXConcat(v, m, mv);
        GXLoadPosMtxImm(mv, GX_PNMTX0);

        GXBegin(GX_QUADS, GX_VTXFMT0, 36);
        DrawFsQuad(8, 7, 2, 3, 0);
        DrawFsQuad(1, 2, 7, 6, 1);
        DrawFsQuad(1, 0, 9, 10, 2);
        DrawFsQuad(4, 1, 10, 11, 1);
        DrawFsQuad(1, 12, 13, 2, 2);
        DrawFsQuad(2, 13, 14, 5, 0);
        DrawFsQuad(18, 15, 16, 17, 2);
        DrawFsQuad(20, 17, 16, 19, 1);
        DrawFsQuad(20, 21, 18, 17, 0);
        GXEnd();
      }
    }
  }
}

static void AnimTick(Mtx v) {
  static u32 ticks = 0;
  Mtx fwd;
  Mtx back;

  u32 animSteps = 100;
  f32 animLoopBack = (f32)STRUT_LN;
  f32 animStepFwd = animLoopBack / animSteps;

  MTXTrans(fwd, 0, 0, animStepFwd);
  MTXTrans(back, 0, 0, -animLoopBack);

  MTXConcat(v, fwd, v);
  if ((ticks % animSteps) == 0)
    MTXConcat(v, back, v);

  ticks++;
}

static void PrintIntro(void) {
  OSReport("\n\n****************************************\n");
  OSReport("to quit:\n");
  OSReport("     hit the start button\n");
  OSReport("****************************************\n");
}
