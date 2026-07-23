#ifndef DOLPHIN_GXGET_H
#define DOLPHIN_GXGET_H

#include <dolphin/gx/GXEnum.h>
#include <dolphin/gx/GXStruct.h>

#ifdef __cplusplus
extern "C" {
#endif

GXBool GXGetTexObjMipMap(GXTexObj* tex_obj);
GXTexFmt GXGetTexObjFmt(GXTexObj* tex_obj);
u16 GXGetTexObjHeight(GXTexObj* tex_obj);
u16 GXGetTexObjWidth(GXTexObj* tex_obj);
GXTexWrapMode GXGetTexObjWrapS(GXTexObj* tex_obj);
GXTexWrapMode GXGetTexObjWrapT(GXTexObj* tex_obj);
void* GXGetTexObjData(GXTexObj* tex_obj);
void* GXGetTexObjUserData(GXTexObj* tex_obj);
void GXGetTexObjAll(GXTexObj* obj, void** image_ptr, u16* width, u16* height, GXTexFmt* format, GXTexWrapMode* wrap_s,
                    GXTexWrapMode* wrap_t, GXBool* mipmap);
void* GXGetTlutObjData(GXTlutObj* tlut_obj);
GXTlutFmt GXGetTlutObjFmt(GXTlutObj* tlut_obj);
u16 GXGetTlutObjNumEntries(GXTlutObj* tlut_obj);
void GXGetTlutObjAll(GXTlutObj* tlut_obj, void** lut, GXTlutFmt* fmt, u16* n_entries);
void GXGetProjectionv(f32* p);
void GXGetViewportv(f32* vp);
void GXGetLightAttnA(GXLightObj* lt_obj, f32* a0, f32* a1, f32* a2);
void GXGetLightAttnK(GXLightObj* lt_obj, f32* k0, f32* k1, f32* k2);
void GXGetLightPos(GXLightObj* lt_obj, f32* x, f32* y, f32* z);
#define GXGetLightPosv(lo, vec) \
    (GXGetLightPos((lo), (f32*)(vec), (f32*)(vec) + 1, (f32*)(vec) + 2))
void GXGetLightDir(GXLightObj* lt_obj, f32* nx, f32* ny, f32* nz);
#define GXGetLightDirv(lo, vec) \
    (GXGetLightDir((lo), (f32*)(vec), (f32*)(vec) + 1, (f32*)(vec) + 2))
void GXGetLightColor(GXLightObj* lt_obj, GXColor* color);
void GXGetVtxDesc(GXAttr attr, GXAttrType* type);
void GXGetVtxDescv(GXVtxDescList* list);
void GXGetVtxAttrFmt(GXVtxFmt idx, GXAttr attr, GXCompCnt* compCnt, GXCompType* compType, u8* shift);
void GXGetVtxAttrFmtv(GXVtxFmt idx, GXVtxAttrFmtList* list);
void GXGetArray(GXAttr attr, void** base_ptr, u8* stride);
void GXGetLineWidth(u8* width, GXTexOffset* tex_offsets);
void GXGetPointSize(u8* size, GXTexOffset* tex_offsets);

#ifdef __cplusplus
}
#endif

#endif
