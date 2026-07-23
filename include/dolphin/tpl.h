#ifndef DOLPHIN_TPL_H
#define DOLPHIN_TPL_H

#include <dolphin/types.h>
#include <dolphin/gx/GXTexture.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* Ptr;

typedef struct {
    u16 numEntries;
    u8 unpacked;
    u8 pad8;
    GXTlutFmt format;
    Ptr data;
} TPLClutHeader, *TPLClutHeaderPtr;

typedef struct {
    u16 height;
    u16 width;
    u32 format;
    Ptr data;
    GXTexWrapMode wrapS;
    GXTexWrapMode wrapT;
    GXTexFilter minFilter;
    GXTexFilter magFilter;
    float LODBias;
    u8 edgeLODEnable;
    u8 minLOD;
    u8 maxLOD;
    u8 unpacked;
} TPLHeader, *TPLHeaderPtr;

typedef struct {
    TPLHeaderPtr textureHeader;
    TPLClutHeaderPtr CLUTHeader;
} TPLDescriptor, *TPLDescriptorPtr;

typedef struct {
    u32 versionNumber;
    u32 numDescriptors;
    TPLDescriptorPtr descriptorArray;
} TPLPalette, *TPLPalettePtr;

void TPLBind(TPLPalettePtr pal);
TPLDescriptorPtr TPLGet(TPLPalettePtr pal, u32 id);
void TPLGetGXTexObjFromPalette(TPLPalettePtr pal, GXTexObj* to, u32 id);
void TPLGetGXTexObjFromPaletteCI(TPLPalettePtr pal, GXTexObj* to, GXTlutObj* tlo, GXTlut tluts, u32 id);

/* GameCube-era convenience wrappers used by gxdemo sources */
typedef TPLPalettePtr TEXPalettePtr;
typedef TPLDescriptorPtr TEXDescriptorPtr;

void TPLGetPalette(TPLPalettePtr* pal, const char* name);
void TPLReleasePalette(TPLPalettePtr* pal);

#define TEXGetPalette TPLGetPalette
#define TEXReleasePalette TPLReleasePalette
#define TEXGet TPLGet
#define TEXGetGXTexObjFromPalette TPLGetGXTexObjFromPalette
#define TEXGetGXTexObjFromPaletteCI TPLGetGXTexObjFromPaletteCI

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_TPL_H */
