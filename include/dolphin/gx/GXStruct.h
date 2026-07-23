#ifndef DOLPHIN_GX_STRUCT_H
#define DOLPHIN_GX_STRUCT_H

#include <dolphin/types.h>
#include <dolphin/gx/GXEnum.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GX_RENDER_MODE_DEFINED
#define GX_RENDER_MODE_DEFINED
typedef struct {
    u32 viTVmode;
    u16 fbWidth;
    u16 efbHeight;
    u16 xfbHeight;
    u16 viXOrigin;
    u16 viYOrigin;
    u16 viWidth;
    u16 viHeight;
    u32 xFBmode;
    u8 field_rendering;
    u8 aa;
    u8 sample_pattern[12][2];
    u8 vfilter[7];
} GXRenderModeObj;
#endif

typedef struct {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
} GXColor;

typedef struct {
    s16 r;
    s16 g;
    s16 b;
    s16 a;
} GXColorS10;

#ifdef TARGET_PC
#define GX_TEXOBJ_DWORDS 22
#define GX_TLUTOBJ_DWORDS 4
#define GX_LIGHTOBJ_DWORDS 16
#define GX_TEXREGION_DWORDS 8
#else
#define GX_TEXOBJ_DWORDS 8
#define GX_TLUTOBJ_DWORDS 3
#define GX_LIGHTOBJ_DWORDS 16
#define GX_TEXREGION_DWORDS 4
#endif

typedef struct {
    u32 data[GX_TEXOBJ_DWORDS];
} GXTexObj;

typedef struct {
    u32 data[GX_TLUTOBJ_DWORDS];
} GXTlutObj;

typedef struct {
    u32 data[GX_LIGHTOBJ_DWORDS];
} GXLightObj;

typedef struct {
    u32 data[GX_TEXREGION_DWORDS];
} GXTexRegion;

typedef struct {
    u32 data[4];
} GXTlutRegion;

typedef struct {
    GXAttr attr;
    GXAttrType type;
} GXVtxDescList;

typedef struct {
    GXAttr attr;
    GXCompCnt cnt;
    GXCompType type;
    u8 frac;
} GXVtxAttrFmtList;

typedef struct {
    u16 r[10];
} GXFogAdjTable;

typedef struct __GXFifoObj GXFifoObj;

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_GX_STRUCT_H */
