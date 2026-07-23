#include <dolphin/tpl.h>

#include <dolphin/dvd.h>
#include <dolphin/os/OSUtil.h>

#include <malloc.h>
#include <stdlib.h>
#include <string.h>

enum { TPL_VERSION = 0x0020AF30 };

static u16 tpl_be16(const void* p) {
    const u8* b = (const u8*)p;
    return (u16)(((u16)b[0] << 8) | (u16)b[1]);
}

static u32 tpl_be32(const void* p) {
    const u8* b = (const u8*)p;
    return ((u32)b[0] << 24) | ((u32)b[1] << 16) | ((u32)b[2] << 8) | (u32)b[3];
}

static float tpl_be_f32(const void* p) {
    u32 bits = tpl_be32(p);
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

static void* tpl_alloc_aligned(size_t size) {
#if defined(_MSC_VER) || defined(_WIN32)
    return _aligned_malloc(size, 32);
#else
    void* p = NULL;
    if (posix_memalign(&p, 32, size) != 0) return NULL;
    return p;
#endif
}

static void tpl_free_aligned(void* p) {
    if (!p) return;
#if defined(_MSC_VER) || defined(_WIN32)
    _aligned_free(p);
#else
    free(p);
#endif
}

static BOOL tpl_try_open(const char* name, DVDFileInfo* dfi) {
    if (!name || !dfi) return FALSE;
    if (DVDOpen(name, dfi)) return TRUE;
    if (name[0] == '/' && DVDOpen(name + 1, dfi)) return TRUE;
    return FALSE;
}

void TPLBind(TPLPalettePtr pal) {
    u8* base;
    u32 num;
    u32 desc_off;
    u8* desc_raw;
    u32 i;

    if (!pal) return;

    base = (u8*)pal;
    pal->versionNumber = tpl_be32(base + 0);
    pal->numDescriptors = tpl_be32(base + 4);
    desc_off = tpl_be32(base + 8);

    if (pal->versionNumber != TPL_VERSION) return;
    if (pal->numDescriptors == 0 || pal->numDescriptors > 4096) return;
    if (desc_off == 0) return;

    pal->descriptorArray = (TPLDescriptorPtr)(base + desc_off);
    num = pal->numDescriptors;
    desc_raw = base + desc_off;

    for (i = 0; i < num; ++i) {
        u32 tex_off = tpl_be32(desc_raw + (i * 8u) + 0);
        u32 clut_off = tpl_be32(desc_raw + (i * 8u) + 4);
        TPLDescriptor* desc = &pal->descriptorArray[i];

        desc->textureHeader = tex_off ? (TPLHeaderPtr)(base + tex_off) : NULL;
        desc->CLUTHeader = clut_off ? (TPLClutHeaderPtr)(base + clut_off) : NULL;

        if (desc->textureHeader) {
            TPLHeader* tex = desc->textureHeader;
            const u8* raw = base + tex_off;
            u32 data_off = tpl_be32(raw + 8);

            tex->height = tpl_be16(raw + 0);
            tex->width = tpl_be16(raw + 2);
            tex->format = tpl_be32(raw + 4);
            tex->data = data_off ? (Ptr)(base + data_off) : NULL;
            tex->wrapS = (GXTexWrapMode)tpl_be32(raw + 12);
            tex->wrapT = (GXTexWrapMode)tpl_be32(raw + 16);
            tex->minFilter = (GXTexFilter)tpl_be32(raw + 20);
            tex->magFilter = (GXTexFilter)tpl_be32(raw + 24);
            tex->LODBias = tpl_be_f32(raw + 28);
            tex->edgeLODEnable = raw[32];
            tex->minLOD = raw[33];
            tex->maxLOD = raw[34];
            tex->unpacked = 1;
        }

        if (desc->CLUTHeader) {
            TPLClutHeader* clut = desc->CLUTHeader;
            const u8* raw = base + clut_off;
            u32 data_off = tpl_be32(raw + 8);

            clut->numEntries = tpl_be16(raw + 0);
            clut->unpacked = 1;
            clut->pad8 = raw[3];
            clut->format = (GXTlutFmt)tpl_be32(raw + 4);
            clut->data = data_off ? (Ptr)(base + data_off) : NULL;
        }
    }
}

TPLDescriptorPtr TPLGet(TPLPalettePtr pal, u32 id) {
    if (!pal || !pal->descriptorArray) return NULL;
    if (id >= pal->numDescriptors) return NULL;
    return &pal->descriptorArray[id];
}

void TPLGetGXTexObjFromPalette(TPLPalettePtr pal, GXTexObj* to, u32 id) {
    TPLDescriptorPtr desc;
    TPLHeader* tex;
    GXBool mipmap;

    if (!to) return;
    desc = TPLGet(pal, id);
    if (!desc || !desc->textureHeader) return;
    tex = desc->textureHeader;

    mipmap = (tex->maxLOD > tex->minLOD) ? GX_TRUE : GX_FALSE;
    if (tex->format == GX_TF_C4 || tex->format == GX_TF_C8 || tex->format == GX_TF_C14X2) {
        GXInitTexObjCI(to, tex->data, tex->width, tex->height, (GXCITexFmt)tex->format, tex->wrapS, tex->wrapT, mipmap,
                       GX_TLUT0);
    } else {
        GXInitTexObj(to, tex->data, tex->width, tex->height, tex->format, tex->wrapS, tex->wrapT, mipmap);
    }
    GXInitTexObjLOD(to, tex->minFilter, tex->magFilter, tex->minLOD / 8.0f, tex->maxLOD / 8.0f, tex->LODBias, GX_FALSE,
                    (GXBool)(tex->edgeLODEnable ? GX_TRUE : GX_FALSE), GX_ANISO_1);
}

void TPLGetGXTexObjFromPaletteCI(TPLPalettePtr pal, GXTexObj* to, GXTlutObj* tlo, GXTlut tluts, u32 id) {
    TPLDescriptorPtr desc;
    TPLHeader* tex;
    TPLClutHeader* clut;
    GXBool mipmap;

    if (!to || !tlo) return;
    desc = TPLGet(pal, id);
    if (!desc || !desc->textureHeader) return;
    tex = desc->textureHeader;
    clut = desc->CLUTHeader;
    if (!clut) return;

    mipmap = (tex->maxLOD > tex->minLOD) ? GX_TRUE : GX_FALSE;

    GXInitTlutObj(tlo, clut->data, clut->format, clut->numEntries);
    GXLoadTlut(tlo, tluts);
    GXInitTexObjCI(to, tex->data, tex->width, tex->height, (GXCITexFmt)tex->format, tex->wrapS, tex->wrapT, mipmap, tluts);
    GXInitTexObjLOD(to, tex->minFilter, tex->magFilter, tex->minLOD / 8.0f, tex->maxLOD / 8.0f, tex->LODBias, GX_FALSE,
                    (GXBool)(tex->edgeLODEnable ? GX_TRUE : GX_FALSE), GX_ANISO_1);
}

void TPLGetPalette(TPLPalettePtr* pal, const char* name) {
    DVDFileInfo dfi;
    void* mem;
    size_t alloc_len;
    s32 read_len;

    if (!pal) return;
    *pal = NULL;

    if (!tpl_try_open(name, &dfi)) return;

    alloc_len = OSRoundUp32B((u32)dfi.length);
    mem = tpl_alloc_aligned(alloc_len);
    if (!mem) {
        DVDClose(&dfi);
        return;
    }
    memset(mem, 0, alloc_len);

    read_len = DVDRead(&dfi, mem, (s32)dfi.length, 0);
    DVDClose(&dfi);
    if (read_len < 0) {
        tpl_free_aligned(mem);
        return;
    }

    *pal = (TPLPalettePtr)mem;
    TPLBind(*pal);
}

void TPLReleasePalette(TPLPalettePtr* pal) {
    if (!pal || !*pal) return;
    tpl_free_aligned(*pal);
    *pal = NULL;
}
