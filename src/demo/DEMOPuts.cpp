/*---------------------------------------------------------------------------*
  Project:  libPorpoise Demo Library
  File:     DEMOPuts.c
  
  Text rendering using DEMOFontBitmap (solid quads per pixel, no texture).
  
  Based on Nintendo's Revolution SDK demo library.
 *---------------------------------------------------------------------------*/

#include <dolphin/demo.h>
#include <dolphin/os.h>
#include <dolphin/mtx.h>
#include <dolphin/gx.h>
#include <dolphin/gx/GXTransform.h>
#include <dolphin/gx/GXGeometry.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHAR_COLS 8
#define CHAR_ROWS 8
#define CHARS_PER_ROW 8
#define CHARS_TOTAL 96  /* 0x20..0x7f */

/* Extract pixel at (col,row) from character bitmap. Each row is 32 bits,
 * 8 nibbles; column c maps to nibble (7-c). */
static inline u32 font_pixel(u32 const* bitmap, s32 charIdx, s32 col, s32 row) {
    if (charIdx < 0 || charIdx >= CHARS_TOTAL || col < 0 || col >= CHAR_COLS || row < 0 || row >= CHAR_ROWS)
        return 0;
    u32 rowBits = bitmap[charIdx * CHAR_ROWS + row];
    return (rowBits >> (4 * (7 - col))) & 0xf;
}

void DEMOSetFontType(s32 type) {
    switch (type) {
    case 1: /* DM_FT_RVS */
        GXSetBlendMode(GX_BM_LOGIC, GX_BL_ZERO, GX_BL_ZERO, GX_LO_INVCOPY);
        break;
    case 2: /* DM_FT_XLU */
        GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_CLEAR);
        break;
    case 0: /* DM_FT_OPQ */
    default:
        GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
        break;
    }
}

void DEMOSetupScrnSpc(s32 width, s32 height, f32 depth) {
    Mtx44 pMtx;
    Mtx mMtx;
    // Screen space: Y top=0 to bottom=height, X left=0 to right=width, Z 0 to -depth
    MTXOrtho(pMtx, 0.0f, (f32)height, 0.0f, (f32)width, 0.0f, -depth);
    GXSetProjection(pMtx, GX_ORTHOGRAPHIC);
    MTXIdentity(mMtx);
    GXLoadPosMtxImm(mMtx, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
}

void DEMOInitCaption(s32 font_type, s32 width, s32 height) {
    DEMOSetupScrnSpc(width, height, 100.0f);
    GXSetZMode(GX_TRUE, GX_ALWAYS, GX_TRUE);
    GXSetNumChans(1);
    GXSetChanCtrl(GX_COLOR0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    {
        GXColor black = {0, 0, 0, 255};
        GXSetChanAmbColor(GX_COLOR0, black);
    }
    {
        GXColor white = {255, 255, 255, 255};
        GXSetChanMatColor(GX_COLOR0, white);
    }
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    DEMOSetFontType(font_type);
}

void DEMOLoadFont(GXTexMapID id, GXTexMtx mtx, DMTexFlt flt) {
    (void)id;
    (void)mtx;
    (void)flt;
    /* Not used: we draw solid quads from DEMOFontBitmap. */
}

static void draw_char_pixels(s16 baseX, s16 baseY, s16 z, s32 charIdx) {
    u32 const* bmp = DEMOFontBitmap;
    s32 count = 0;
    for (s32 row = 0; row < CHAR_ROWS; row++) {
        for (s32 col = 0; col < CHAR_COLS; col++) {
            if (font_pixel(bmp, charIdx, col, row)) count++;
        }
    }
    if (count == 0) return;

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);

    GXBegin(GX_QUADS, GX_VTXFMT0, (u16)(count * 4));

    for (s32 row = 0; row < CHAR_ROWS; row++) {
        for (s32 col = 0; col < CHAR_COLS; col++) {
            if (!font_pixel(bmp, charIdx, col, row)) continue;
            f32 px = (f32)(baseX + col);
            f32 py = (f32)(baseY + row);
            f32 qx = px + 1.0f;
            f32 qy = py + 1.0f;
            GXPosition3f32(px, py, (f32)z);
            GXPosition3f32(qx, py, (f32)z);
            GXPosition3f32(qx, qy, (f32)z);
            GXPosition3f32(px, qy, (f32)z);
        }
    }
    GXEnd();
}

void DEMOPuts(s16 x, s16 y, s16 z, char* str) {
    if (!str) return;

    char* s = str;
    s16 curX = x;
    s16 curY = y;

    while (*s) {
        int c = (unsigned char)*s++;
        if (c == '\n') {
            curY += CHAR_ROWS;
            curX = x;
            continue;
        }
        if (c >= 0x20 && c <= 0x7f) {
            s32 idx = c - 0x20;
            draw_char_pixels(curX, curY, z, idx);
            curX += CHAR_COLS;
        }
    }
}

void DEMOPrintf(s16 x, s16 y, s16 z, char* fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    DEMOPuts(x, y, z, buffer);
}

OSFontHeader* DEMOInitROMFont(void) {
    // Stub
    return NULL;
}

void DEMOSetROMFontSize(s16 size, s16 space) {
    (void)size; (void)space;
    // Stub
}

void DEMOGetROMFontSize(s16* size, s16* space) {
    if (size) *size = 0;
    if (space) *space = 0;
    // Stub
}

int DEMOGetRFTextWidth(char* string) {
    (void)string;
    return 0; // Stub
}

int DEMOGetRFTextHeight(char* string) {
    (void)string;
    return 0; // Stub
}

int DEMORFPuts(s16 x, s16 y, s16 z, char* string) {
    (void)x; (void)y; (void)z;
    if (string) {
        OSReport("%s\n", string);
    }
    return 0; // Stub
}

int DEMORFPutsEx(s16 x, s16 y, s16 z, char* string, s16 maxWidth, int length) {
    (void)x; (void)y; (void)z; (void)maxWidth; (void)length;
    if (string) {
        OSReport("%s\n", string);
    }
    return 0; // Stub
}

int DEMORFPrintf(s16 x, s16 y, s16 z, char* fmt, ...) {
    (void)x; (void)y; (void)z;
    va_list args;
    va_start(args, fmt);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    OSReport("%s\n", buffer);
    va_end(args);
    return 0; // Stub
}

char* DEMODumpROMFont(char* string) {
    (void)string;
    return NULL; // Stub
}

#ifdef __cplusplus
} // extern "C"
#endif

