#include <demo.h>
#include <dolphin.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static s32 fontShift = 0;
static GXTexObj fontTexObj;

void DEMOSetFontType(s32 attr) {

  switch (attr) {
  case DM_FT_RVS:

    GXSetBlendMode(GX_BM_LOGIC, GX_BL_ZERO, GX_BL_ZERO, GX_LO_INVCOPY);
    break;

  case DM_FT_XLU:

    GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_CLEAR);
    break;

  case DM_FT_OPQ:
  default:

    GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
    break;
  }
  return;
}

void DEMOLoadFont(GXTexMapID texMap, GXTexMtx texMtx, DMTexFlt texFlt) {
  Mtx fontTMtx;
  u16 width = 64;
  u16 height = (u16)((0x80 - 0x20) * 8 * 8 / width);

  GXInitTexObj(&fontTexObj, DEMOFontBitmap, width, height, GX_TF_I4, GX_CLAMP,
               GX_CLAMP, GX_FALSE);

  if (texFlt == DMTF_POINTSAMPLE) {
    GXInitTexObjLOD(&fontTexObj, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, GX_DISABLE,
                    GX_FALSE, GX_ANISO_1);
    fontShift = 0;
  } else {
    fontShift = 1;
  }

  GXLoadTexObj(&fontTexObj, texMap);

  MTXScale(fontTMtx, 1.0f / (float)width, 1.0f / (float)height, 1.0f);

  GXLoadTexMtxImm(fontTMtx, texMtx, GX_MTX2x4);
  GXSetNumTexGens(1);
  GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, texMtx);

  return;
}

void DEMOSetupScrnSpc(s32 width, s32 height, float depth) {
  Mtx44 pMtx;
  Mtx mMtx;
  f32 top;

  if (DEMOGetRenderModeObj()->field_rendering && !VIGetNextField()) {
    top = -0.667F;
  } else {
    top = 0.00F;
  }

  MTXOrtho(pMtx, top, (float)height, 0.0f, (float)width, 0.0f, -depth);
  GXSetProjection(pMtx, GX_ORTHOGRAPHIC);
  MTXIdentity(mMtx);
  GXLoadPosMtxImm(mMtx, GX_PNMTX0);
  GXSetCurrentMtx(GX_PNMTX0);

  return;
}

void DEMOInitCaption(s32 font_type, s32 width, s32 height) {

  DEMOSetupScrnSpc(width, height, 100.0f);

  GXSetZMode(GX_ENABLE, GX_ALWAYS, GX_ENABLE);

  GXSetNumChans(0);
  GXSetNumTevStages(1);
  GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);
  GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);

  DEMOLoadFont(GX_TEXMAP0, GX_TEXMTX0, DMTF_POINTSAMPLE);

  DEMOSetFontType(font_type);

  return;
}

void DEMOPuts(s16 x, s16 y, s16 z, char *string) {
  char *str = string;
  s32 s, t;
  s32 c, w, len, i;

  GXClearVtxDesc();
  GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
  GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S16, 1);

  len = 0;
  while (1) {

    c = *str++;
    if (' ' <= c && c <= 0x7f) {
      len++;
    }

    else {
      if (len > 0) {

        GXBegin(GX_QUADS, GX_VTXFMT0, (u16)(len * 4));
        for (i = 0; i < len; i++) {

          w = string[i] - ' ';
          s = (w % 8) * 16 + fontShift;
          t = (w / 8) * 16 + fontShift;
          GXPosition3s16((s16)(i * 8 + x), (s16)(y), z);
          GXTexCoord2s16((s16)(s), (s16)(t));
          GXPosition3s16((s16)(i * 8 + x + 8), (s16)(y), z);
          GXTexCoord2s16((s16)(s + 16), (s16)(t));
          GXPosition3s16((s16)(i * 8 + x + 8), (s16)(y + 8), z);
          GXTexCoord2s16((s16)(s + 16), (s16)(t + 16));
          GXPosition3s16((s16)(i * 8 + x), (s16)(y + 8), z);
          GXTexCoord2s16((s16)(s), (s16)(t + 16));
        }
        GXEnd();
        len = 0;
      }
      string = str;

      if (c == '\n')
        y += 8;

      else
        break;
    }
  }
  return;
}

void DEMOPrintf(s16 x, s16 y, s16 z, char *fmt, ...) {
  va_list vlist;
  char buf[256];

  va_start(vlist, fmt);
  vsprintf(buf, fmt, vlist);
  va_end(vlist);

  DEMOPuts(x, y, z, buf);

  return;
}

static OSFontHeader *FontData;
static void *LastSheet;
static s16 FontSize;
static s16 FontSpace;

OSFontHeader *DEMOInitROMFont(void) {
  switch (OSGetFontEncode()) {
  case OS_FONT_ENCODE_SJIS:
    FontData = OSAlloc(OS_FONT_SIZE_SJIS);
    break;
  case OS_FONT_ENCODE_ANSI:
    FontData = OSAlloc(OS_FONT_SIZE_ANSI);
    break;
  default:
    FontData = OSAlloc(OS_FONT_SIZE_UTF);
    break;
  }
  if (!FontData) {
    OSHalt("Ins. memory to load ROM font.");
  }
  if (!OSInitFont(FontData)) {
    OSHalt("ROM font is available in boot ROM ver 0.8 or later.");
  }

  FontSize = (s16)(FontData->cellWidth * 16);
  FontSpace = -16;

  return FontData;
}

void DEMOSetROMFontSize(s16 size, s16 space) {
  FontSize = (s16)(size * 16);
  FontSpace = (s16)(space * 16);
}

void DEMOGetROMFontSize(s16 *size, s16 *space) {
  if (size) {
    *size = (s16)(FontSize / 16);
  }
  if (space) {
    *space = (s16)(FontSpace / 16);
  }
}

static void DrawFontChar(int x, int y, int z, int xChar, int yChar) {
  s16 posLeft = (s16)x;
  s16 posRight = (s16)(posLeft + FontSize);
  s16 posTop = (s16)(y - (FontData->ascent * FontSize / FontData->cellWidth));
  s16 posBottom =
      (s16)(y + (FontData->descent * FontSize / FontData->cellWidth));

  s16 texLeft = (s16)xChar;
  s16 texRight = (s16)(xChar + FontData->cellWidth);
  s16 texTop = (s16)yChar;
  s16 texBottom = (s16)(yChar + FontData->cellHeight);

  GXBegin(GX_QUADS, GX_VTXFMT0, 4);
  GXPosition3s16(posLeft, posTop, (s16)z);
  GXTexCoord2s16(texLeft, texTop);

  GXPosition3s16(posRight, posTop, (s16)z);
  GXTexCoord2s16(texRight, texTop);

  GXPosition3s16(posRight, posBottom, (s16)z);
  GXTexCoord2s16(texRight, texBottom);

  GXPosition3s16(posLeft, posBottom, (s16)z);
  GXTexCoord2s16(texLeft, texBottom);
  GXEnd();
}

static void LoadSheet(void *image, GXTexMapID texMapID) {
  Mtx mtx;
  GXTexObj texObj;

  if (LastSheet == image) {
    return;
  }
  LastSheet = image;

  GXInitTexObj(&texObj, image, FontData->sheetWidth, FontData->sheetHeight,
               (GXTexFmt)FontData->sheetFormat, GX_CLAMP, GX_CLAMP, GX_FALSE);

  GXInitTexObjLOD(&texObj, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_DISABLE,
                  GX_FALSE, GX_ANISO_1);

  GXLoadTexObj(&texObj, texMapID);
  MTXScale(mtx, 1.0f / FontData->sheetWidth, 1.0f / FontData->sheetHeight,
           1.0f);
  GXLoadTexMtxImm(mtx, GX_TEXMTX0, GX_MTX2x4);
  GXSetNumTexGens(1);
  GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX0);
}

int DEMORFPuts(s16 x, s16 y, s16 z, char *string) {
  s32 cx;
  void *image;
  s32 xChar;
  s32 yChar;
  s32 width;

  ASSERT(FontData);

  LastSheet = 0;

  GXClearVtxDesc();
  GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
  GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 4);
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S16, 0);

  x *= 16;
  y *= 16;
  z *= 16;

  width = 0;
  while (*string) {
    if (*string == '\n') {
      width = 0;
      y += FontData->leading * FontSize / FontData->cellWidth;
      ++string;
      continue;
    }

    if (*string == '\t') {
      width += 8 * (FontSize + FontSpace);
      width -= width % (8 * (FontSize + FontSpace));
      ++string;
      continue;
    }

    string = OSGetFontTexture(string, &image, &xChar, &yChar, &cx);

    LoadSheet(image, GX_TEXMAP0);
    DrawFontChar(x + width, y, z, xChar, yChar);
    width += FontSize * cx / FontData->cellWidth + FontSpace;
  }
  return (width + 15) / 16;
}

int DEMORFPutsEx(s16 x, s16 y, s16 z, char *string, s16 maxWidth, int length) {
  s32 cx;
  void *image;
  s32 xChar;
  s32 yChar;
  s32 width;
  char *end;

  ASSERT(FontData);

  LastSheet = 0;

  GXClearVtxDesc();
  GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
  GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 4);
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S16, 0);

  x *= 16;
  y *= 16;
  z *= 16;
  maxWidth *= 16;

  end = string + length;
  width = 0;
  while (*string && string < end) {

    if (*string == '\n') {
      width = 0;
      y += FontData->leading * FontSize / FontData->cellWidth;
      ++string;
      continue;
    }

    string = OSGetFontTexture(string, &image, &xChar, &yChar, &cx);

    if (maxWidth < width + FontSize * cx / FontData->cellWidth + FontSpace) {
      width = 0;
      y += FontData->leading * FontSize / FontData->cellWidth;
    }

    LoadSheet(image, GX_TEXMAP0);
    DrawFontChar(x + width, y, z, xChar, yChar);
    width += FontSize * cx / FontData->cellWidth + FontSpace;
  }
  return (width + 15) / 16;
}

int DEMORFPrintf(s16 x, s16 y, s16 z, char *fmt, ...) {
  va_list vlist;
  char buf[256];

  va_start(vlist, fmt);
  vsprintf(buf, fmt, vlist);
  va_end(vlist);

  return DEMORFPuts(x, y, z, buf);
}

char *DEMODumpROMFont(char *string) {
  u32 image[48 / 2 * 48 / 4];
  void *temp;
  int i, j;
  s32 width;

  ASSERT(FontData);

  switch (OSGetFontEncode()) {
  case OS_FONT_ENCODE_SJIS:
    temp = (u8 *)FontData + OS_FONT_SIZE_SJIS - OS_FONT_ROM_SIZE_SJIS;
    break;
  case OS_FONT_ENCODE_ANSI:
    temp = (u8 *)FontData + OS_FONT_SIZE_ANSI - OS_FONT_ROM_SIZE_ANSI;
    break;
  default:
    temp = (u8 *)FontData + OS_FONT_SIZE_UTF - OS_FONT_ROM_SIZE_UTF;
    break;
  }
  temp = (void *)OSRoundDown32B(temp);
  OSLoadFont(FontData, temp);

  memset(image, 0x00, sizeof(image));

  string = OSGetFontTexel(string, image, 0, 48 / 4, &width);

  for (i = 0; i < 48; i++) {
    j = 48 * (i / 8) + (i % 8);
    OSReport("%08x%08x%08x%08x%08x%08x\n", image[j], image[j + 32 / 4],
             image[j + 64 / 4], image[j + 96 / 4], image[j + 128 / 4],
             image[j + 160 / 4]);
  }

  OSReport("\nwidth %d\n", width);

  OSInitFont(FontData);

  return string;
}

int DEMOGetRFTextWidth(char *string) {
  s32 cx;
  s32 width;
  s32 maxWidth;

  ASSERT(FontData);

  maxWidth = width = 0;
  while (*string) {
    if (*string == '\n') {
      if (maxWidth < width) {
        maxWidth = width;
      }
      width = 0;
    }
    string = OSGetFontWidth(string, &cx);
    width += FontSize * cx / FontData->cellWidth + FontSpace;
  }
  if (maxWidth < width) {
    maxWidth = width;
  }
  return (maxWidth + 15) / 16;
}

int DEMOGetRFTextHeight(char *string) {
  s32 height;

  ASSERT(FontData);

  height = 1;
  while (*string) {
    if (*string == '\n') {
      ++height;
    }
    ++string;
  }
  height *= FontData->leading * FontSize / FontData->cellWidth;
  return (height + 15) / 16;
}
