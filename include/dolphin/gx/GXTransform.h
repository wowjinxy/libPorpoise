#ifndef DOLPHIN_GXTRANSFORM_H
#define DOLPHIN_GXTRANSFORM_H

#include <dolphin/gx/GXEnum.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GX_PROJECTION_SZ 7
#define GX_VIEWPORT_SZ   6

void GXSetProjection(f32 mtx[4][4], GXProjectionType type);
void GXSetProjectionv(const f32* ptr);
void GXLoadPosMtxImm(f32 mtx[3][4], u32 id);
void GXLoadNrmMtxImm(f32 mtx[3][4], u32 id);
void GXLoadNrmMtxImm3x3(f32 mtx[3][3], u32 id);
void GXLoadTexMtxImm(f32 mtx[][4], u32 id, GXTexMtxType type);
void GXLoadPosMtxIndx(u16 index, u32 id);
void GXLoadNrmMtxIndx3x3(u16 index, u32 id);
void GXLoadTexMtxIndx(u16 index, u32 id, GXTexMtxType type);
void GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz);
void GXSetViewportv(const f32* vp);
void GXGetViewportv(f32* vp);
void GXSetCurrentMtx(u32 id);
void GXSetViewportJitter(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz, u32 field);
void GXSetScissorBoxOffset(s32 x_off, s32 y_off);
void GXSetClipMode(GXClipMode mode);
void GXGetProjectionv(f32* p);
void GXProject(f32 x, f32 y, f32 z, f32 mtx[3][4], f32* pm, f32* vp, f32* sx, f32* sy, f32* sz);

#ifdef __cplusplus
}
#endif

#endif
