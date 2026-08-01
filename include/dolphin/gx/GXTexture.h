#ifndef _DOLPHIN_GXTEXTURE_H
#define _DOLPHIN_GXTEXTURE_H

#include <dolphin/types.h>

#include <dolphin/gx/GXEnum.h>
#include <dolphin/gx/GXTypes.h>

BEGIN_SCOPE_EXTERN_C

//////////// TEXTURE CALLBACKS /////////////

typedef GXTexRegion* (*GXTexRegionCallback)(const GXTexObj* t_obj, GXTexMapID id);
typedef GXTlutRegion* (*GXTlutRegionCallback)(u32 idx);

////////////////////////////////////////////

//////////// TEXTURE FUNCTIONS /////////////
// Init functions.
extern void GXInitTexObj(GXTexObj* obj, void* imagePtr, u16 width, u16 height, GXTexFmt format, GXTexWrapMode sWrap, GXTexWrapMode tWrap,
                         GXBool useMIPmap);
#ifdef LIBPORPOISE_PORT
/* Texture files and byte-oriented buffers remain canonical GameCube memory.
 * Use this initializer when host code instead built the texture as native
 * u16 scalar words. The void pointer keeps the explicit path available after
 * middleware has erased the source's element type. */
extern void GXInitTexObjHostNativeU16(
    GXTexObj* obj, void* imagePtr, u16 width, u16 height, GXTexFmt format,
    GXTexWrapMode sWrap, GXTexWrapMode tWrap, GXBool useMIPmap);

/* Preserve SDK source compatibility for direct u16 texture arrays while
 * leaving u8, void, and serialized TPL pointers in canonical byte order. */
#if defined(__cplusplus)
#define LIBPORPOISE_GX_TEX_IS_NATIVE_U16(imagePtr) \
    (__is_same(decltype((imagePtr) + 0), u16*) || \
     __is_same(decltype((imagePtr) + 0), const u16*) || \
     __is_same(decltype((imagePtr) + 0), volatile u16*) || \
     __is_same(decltype((imagePtr) + 0), const volatile u16*))
#define GXInitTexObj(obj, imagePtr, width, height, format, sWrap, tWrap, useMIPmap) \
    (LIBPORPOISE_GX_TEX_IS_NATIVE_U16(imagePtr) \
         ? GXInitTexObjHostNativeU16( \
               (obj), (void*)(imagePtr), (width), (height), (format), \
               (sWrap), (tWrap), (useMIPmap)) \
         : GXInitTexObj( \
               (obj), (void*)(imagePtr), (width), (height), (format), \
               (sWrap), (tWrap), (useMIPmap)))
#elif defined(__GNUC__) || defined(__clang__)
#define LIBPORPOISE_GX_TEX_IS_NATIVE_U16(imagePtr) \
    (__builtin_types_compatible_p(__typeof__((imagePtr) + 0), u16*) || \
     __builtin_types_compatible_p( \
         __typeof__((imagePtr) + 0), const u16*) || \
     __builtin_types_compatible_p( \
         __typeof__((imagePtr) + 0), volatile u16*) || \
     __builtin_types_compatible_p( \
         __typeof__((imagePtr) + 0), const volatile u16*))
#define GXInitTexObj(obj, imagePtr, width, height, format, sWrap, tWrap, useMIPmap) \
    (LIBPORPOISE_GX_TEX_IS_NATIVE_U16(imagePtr) \
         ? GXInitTexObjHostNativeU16( \
               (obj), (void*)(imagePtr), (width), (height), (format), \
               (sWrap), (tWrap), (useMIPmap)) \
         : GXInitTexObj( \
               (obj), (void*)(imagePtr), (width), (height), (format), \
               (sWrap), (tWrap), (useMIPmap)))
#endif
#endif
extern void GXInitTexObjCI(GXTexObj* obj, void* imagePtr, u16 width, u16 height, GXCITexFmt format, GXTexWrapMode sWrap,
                           GXTexWrapMode tWrap, GXBool useMIPmap, u32 tlutName);
extern void GXInitTexObjLOD(GXTexObj* obj, GXTexFilter minFilter, GXTexFilter maxFilter, f32 minLOD, f32 maxLOD, f32 lodBias,
                            GXBool doBiasClamp, GXBool doEdgeLOD, GXAnisotropy maxAniso);
extern void GXInitTexObjFilter(GXTexObj* obj, GXTexFilter minFilter, GXTexFilter magFilter);
extern void GXInitTexObjMaxLOD(GXTexObj* obj, f32 maxLOD);
extern void GXInitTexObjMinLOD(GXTexObj* obj, f32 minLOD);
extern void GXInitTexObjLODBias(GXTexObj* obj, f32 lodBias);
extern void GXInitTexObjBiasClamp(GXTexObj* obj, GXBool doBiasClamp);
extern void GXInitTexObjEdgeLOD(GXTexObj* obj, GXBool doEdgeLOD);
extern void GXInitTexObjMaxAniso(GXTexObj* obj, GXAnisotropy maxAniso);
extern void GXInitTexObjUserData(GXTexObj* obj, void* userData);

// Get functions.
extern void GXGetTexObjAll(const GXTexObj* obj, void** imagePtr, u16* width, u16* height, GXTexFmt* format,
                           GXTexWrapMode* sWrap, GXTexWrapMode* tWrap, GXBool* useMIPmap);
extern GXTexFmt GXGetTexObjFmt(const GXTexObj* obj);
extern GXBool GXGetTexObjMipMap(const GXTexObj* obj);
extern void* GXGetTexObjData(const GXTexObj* obj);
extern void* GXGetTexObjUserData(const GXTexObj* obj);
extern u16 GXGetTexObjWidth(const GXTexObj* obj);
extern u16 GXGetTexObjHeight(const GXTexObj* obj);
extern GXTexWrapMode GXGetTexObjWrapS(const GXTexObj* obj);
extern GXTexWrapMode GXGetTexObjWrapT(const GXTexObj* obj);
extern void GXGetTexObjLODAll(const GXTexObj* obj, GXTexFilter* minFilter, GXTexFilter* magFilter,
                              f32* minLOD, f32* maxLOD, f32* lodBias, GXBool* doBiasClamp,
                              GXBool* doEdgeLOD, GXAnisotropy* maxAniso);
extern GXTexFilter GXGetTexObjMinFilt(const GXTexObj* obj);
extern GXTexFilter GXGetTexObjMagFilt(const GXTexObj* obj);
extern f32 GXGetTexObjMinLOD(const GXTexObj* obj);
extern f32 GXGetTexObjMaxLOD(const GXTexObj* obj);
extern f32 GXGetTexObjLODBias(const GXTexObj* obj);
extern GXBool GXGetTexObjBiasClamp(const GXTexObj* obj);
extern GXBool GXGetTexObjEdgeLOD(const GXTexObj* obj);
extern GXAnisotropy GXGetTexObjMaxAniso(const GXTexObj* obj);
extern u32 GXGetTexObjTlut(const GXTexObj* obj);
extern u32 GXGetTexBufferSize(u16 width, u16 height, u32 format, GXBool mipmap, u8 max_lod);

// Load functions.
extern void GXLoadTexObjPreLoaded(GXTexObj* obj, GXTexRegion* region, GXTexMapID map);
extern void GXLoadTexObj(GXTexObj* obj, GXTexMapID map);

// Tlut functions.
extern void GXInitTlutObj(GXTlutObj* obj, void* table, GXTlutFmt format, u16 numEntries);
#ifdef LIBPORPOISE_PORT
/* Host middleware often builds palettes as native u16 scalar values. This
 * explicit initializer prevents their byte representation from being
 * mistaken for canonical GameCube big-endian TLUT memory. */
extern void GXInitTlutObjHostNativeU16(GXTlutObj* obj, u16* table,
                                      GXTlutFmt format, u16 numEntries);

/* Preserve source compatibility with SDK code that passes a u16 palette
 * directly to GXInitTlutObj. On GameCube those scalar values reside in
 * big-endian memory; on a little-endian host they need canonicalizing when
 * the simulated TLUT DMA occurs. Byte-oriented pointers continue to mean an
 * already-serialized GameCube buffer (for example, data from a TPL file). */
#if defined(__cplusplus)
#define LIBPORPOISE_GX_TLUT_IS_NATIVE_U16(table) \
    (__is_same(decltype((table) + 0), u16*) || \
     __is_same(decltype((table) + 0), const u16*) || \
     __is_same(decltype((table) + 0), volatile u16*) || \
     __is_same(decltype((table) + 0), const volatile u16*))
#define GXInitTlutObj(obj, table, format, numEntries) \
    (LIBPORPOISE_GX_TLUT_IS_NATIVE_U16(table) \
         ? GXInitTlutObjHostNativeU16( \
               (obj), (u16*)(table), (format), (numEntries)) \
         : GXInitTlutObj( \
               (obj), (void*)(table), (format), (numEntries)))
#elif defined(__GNUC__) || defined(__clang__)
#define LIBPORPOISE_GX_TLUT_IS_NATIVE_U16(table) \
    (__builtin_types_compatible_p(__typeof__((table) + 0), u16*) || \
     __builtin_types_compatible_p(__typeof__((table) + 0), const u16*) || \
     __builtin_types_compatible_p(__typeof__((table) + 0), volatile u16*) || \
     __builtin_types_compatible_p( \
         __typeof__((table) + 0), const volatile u16*))
#define GXInitTlutObj(obj, table, format, numEntries) \
    (LIBPORPOISE_GX_TLUT_IS_NATIVE_U16(table) \
         ? GXInitTlutObjHostNativeU16( \
               (obj), (u16*)(table), (format), (numEntries)) \
         : GXInitTlutObj( \
               (obj), (void*)(table), (format), (numEntries)))
#endif
#endif
extern void GXLoadTlut(GXTlutObj* obj, u32 tlutName);
extern void GXGetTlutObjAll(const GXTlutObj* obj, void** table,
                            GXTlutFmt* format, u16* numEntries);
extern void* GXGetTlutObjData(const GXTlutObj* obj);
extern GXTlutFmt GXGetTlutObjFmt(const GXTlutObj* obj);
extern u16 GXGetTlutObjNumEntries(const GXTlutObj* obj);

// Region functions.
extern void GXInitTexCacheRegion(GXTexRegion* region, GXBool is32bMIPmap, u32 memEven, GXTexCacheSize sizeEven, u32 memOdd,
                                 GXTexCacheSize sizeOdd);
extern void GXInitTlutRegion(GXTlutRegion* region, u32 memAddr, GXTlutSize tlutSize);
extern void GXInitTexPreLoadRegion(GXTexRegion* region, u32 tmemEven, u32 sizeEven,
                                   u32 tmemOdd, u32 sizeOdd);

// Other functions.
extern void GXInvalidateTexAll();
extern void GXInvalidateTexRegion(const GXTexRegion* region);
extern void GXPreLoadEntireTexture(const GXTexObj* obj, const GXTexRegion* region);
extern void GXSetTexCoordScaleManually(GXTexCoordID coord, GXBool enable,
                                       u16 scaleS, u16 scaleT);
extern GXTexRegionCallback GXSetTexRegionCallback(GXTexRegionCallback func);
extern GXTlutRegionCallback GXSetTlutRegionCallback(GXTlutRegionCallback func);

// Unknown arg functions.
// TODO: work these out.
extern void __GXSetSUTexRegs();
extern void __GXSetTmemConfig(u32 config);

// Unused/inlined in P2.
extern void GXInitTexObjData(GXTexObj* obj, void* imagePtr);
extern void GXInitTexObjWrapMode(GXTexObj* obj, GXTexWrapMode sWrap, GXTexWrapMode tWrap);
extern void GXInitTexObjTlut(GXTexObj* obj, u32 tlutName);
// TODO: finish filling these out for reference purposes.

extern void __GetImageTileCount(GXTexFmt format, u16 width, u16 height, u32* a, u32* b, u32* c);

////////////////////////////////////////////

END_SCOPE_EXTERN_C

#endif
