#include <dolphin/tpl.h>

#include <dolphin/dvd.h>
#include <dolphin/os/OSHostEndian.h>
#include <dolphin/os/OSUtil.h>

#include <malloc.h>
#include <stdlib.h>
#include <string.h>

enum { TPL_VERSION = 0x0020AF30 };
enum { TPL_FALLBACK_DESCRIPTOR_COUNT = 256 };
enum {
    TPL_ALLOCATION_FALLBACK = 0x46424B50,
    TPL_ALLOCATION_LOADED = 0x4C4F4144,
};

typedef struct {
    TPLPalette palette;
    u32 allocationMagic;
    TPLDescriptor descriptors[TPL_FALLBACK_DESCRIPTOR_COUNT];
    TPLHeader headers[TPL_FALLBACK_DESCRIPTOR_COUNT];
} TPLFallbackPalette;

typedef struct {
    TPLPalette palette;
    u32 allocationMagic;
    void* rawData;
    size_t rawSize;
    TPLDescriptor* descriptors;
    TPLHeader* headers;
    TPLClutHeader* cluts;
} TPLLoadedPalette;

static void* tpl_alloc_aligned(size_t size);
static void tpl_free_aligned(void* p);
static TPLPalettePtr tpl_parse_loaded_palette(void* rawData, size_t rawSize);

static u8 s_fallback_texture[32] ATTRIBUTE_ALIGN(32) = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static void tpl_init_fallback_texture(GXTexObj* to) {
    GXInitTexObj(
        to,
        s_fallback_texture,
        1,
        1,
        GX_TF_I8,
        GX_CLAMP,
        GX_CLAMP,
        GX_FALSE);
    GXInitTexObjLOD(
        to,
        GX_NEAR,
        GX_NEAR,
        0.0f,
        0.0f,
        0.0f,
        GX_FALSE,
        GX_FALSE,
        GX_ANISO_1);
}

static TPLPalettePtr tpl_create_fallback_palette(void) {
    TPLFallbackPalette* fallback =
        (TPLFallbackPalette*)tpl_alloc_aligned(sizeof(TPLFallbackPalette));
    u32 index;

    if (!fallback) return NULL;
    memset(fallback, 0, sizeof(*fallback));
    fallback->palette.versionNumber = TPL_VERSION;
    fallback->palette.numDescriptors = TPL_FALLBACK_DESCRIPTOR_COUNT;
    fallback->palette.descriptorArray = fallback->descriptors;
    fallback->allocationMagic = TPL_ALLOCATION_FALLBACK;

    for (index = 0; index < TPL_FALLBACK_DESCRIPTOR_COUNT; ++index) {
        TPLHeader* header = &fallback->headers[index];
        fallback->descriptors[index].textureHeader = header;
        header->height = 1;
        header->width = 1;
        header->format = GX_TF_I8;
        header->data = s_fallback_texture;
        header->wrapS = GX_CLAMP;
        header->wrapT = GX_CLAMP;
        header->minFilter = GX_NEAR;
        header->magFilter = GX_NEAR;
        header->LODBias = 0.0f;
        header->edgeLODEnable = GX_FALSE;
        header->minLOD = 0;
        header->maxLOD = 0;
        header->unpacked = 1;
    }

    return &fallback->palette;
}

static BOOL tpl_range_valid(size_t offset, size_t length, size_t total) {
    return offset <= total && length <= total - offset;
}

static TPLPalettePtr tpl_parse_loaded_palette(void* rawData, size_t rawSize) {
    const u8* raw = (const u8*)rawData;
    TPLLoadedPalette* loaded;
    u8* cursor;
    u32 descriptorOffset;
    u32 descriptorCount;
    u32 i;
    size_t allocationSize;

    if (!raw || rawSize < 12 || OSReadBigEndian32(raw) != TPL_VERSION) return NULL;

    descriptorCount = OSReadBigEndian32(raw + 4);
    descriptorOffset = OSReadBigEndian32(raw + 8);
    if (descriptorCount == 0 || descriptorCount > 4096) return NULL;
    if (!tpl_range_valid(descriptorOffset, (size_t)descriptorCount * 8u, rawSize)) return NULL;

    allocationSize = sizeof(TPLLoadedPalette) +
                     (size_t)descriptorCount * sizeof(TPLDescriptor) +
                     (size_t)descriptorCount * sizeof(TPLHeader) +
                     (size_t)descriptorCount * sizeof(TPLClutHeader);
    loaded = (TPLLoadedPalette*)tpl_alloc_aligned(allocationSize);
    if (!loaded) return NULL;
    memset(loaded, 0, allocationSize);

    cursor = (u8*)(loaded + 1);
    loaded->descriptors = (TPLDescriptor*)cursor;
    cursor += (size_t)descriptorCount * sizeof(TPLDescriptor);
    loaded->headers = (TPLHeader*)cursor;
    cursor += (size_t)descriptorCount * sizeof(TPLHeader);
    loaded->cluts = (TPLClutHeader*)cursor;

    loaded->palette.versionNumber = TPL_VERSION;
    loaded->palette.numDescriptors = descriptorCount;
    loaded->palette.descriptorArray = loaded->descriptors;
    loaded->allocationMagic = TPL_ALLOCATION_LOADED;
    loaded->rawData = rawData;
    loaded->rawSize = rawSize;

    for (i = 0; i < descriptorCount; ++i) {
        const u8* descriptor = raw + descriptorOffset + (size_t)i * 8u;
        u32 textureOffset = OSReadBigEndian32(descriptor + 0);
        u32 clutOffset = OSReadBigEndian32(descriptor + 4);

        if (textureOffset != 0) {
            const u8* texture;
            u32 dataOffset;
            TPLHeader* header;
            if (!tpl_range_valid(textureOffset, 36, rawSize)) {
                tpl_free_aligned(loaded);
                return NULL;
            }
            texture = raw + textureOffset;
            dataOffset = OSReadBigEndian32(texture + 8);
            if (dataOffset != 0 && !tpl_range_valid(dataOffset, 1, rawSize)) {
                tpl_free_aligned(loaded);
                return NULL;
            }

            header = &loaded->headers[i];
            loaded->descriptors[i].textureHeader = header;
            header->height = OSReadBigEndian16(texture + 0);
            header->width = OSReadBigEndian16(texture + 2);
            header->format = OSReadBigEndian32(texture + 4);
            header->data = dataOffset ? (Ptr)(raw + dataOffset) : NULL;
            header->wrapS = (GXTexWrapMode)OSReadBigEndian32(texture + 12);
            header->wrapT = (GXTexWrapMode)OSReadBigEndian32(texture + 16);
            header->minFilter = (GXTexFilter)OSReadBigEndian32(texture + 20);
            header->magFilter = (GXTexFilter)OSReadBigEndian32(texture + 24);
            header->LODBias = OSReadBigEndianF32(texture + 28);
            header->edgeLODEnable = texture[32];
            header->minLOD = texture[33];
            header->maxLOD = texture[34];
            header->unpacked = 1;
        }

        if (clutOffset != 0) {
            const u8* clut;
            u32 dataOffset;
            TPLClutHeader* header;
            if (!tpl_range_valid(clutOffset, 12, rawSize)) {
                tpl_free_aligned(loaded);
                return NULL;
            }
            clut = raw + clutOffset;
            dataOffset = OSReadBigEndian32(clut + 8);
            if (dataOffset != 0 && !tpl_range_valid(dataOffset, 1, rawSize)) {
                tpl_free_aligned(loaded);
                return NULL;
            }

            header = &loaded->cluts[i];
            loaded->descriptors[i].CLUTHeader = header;
            header->numEntries = OSReadBigEndian16(clut + 0);
            header->unpacked = 1;
            header->pad8 = clut[3];
            header->format = (GXTlutFmt)OSReadBigEndian32(clut + 4);
            header->data = dataOffset ? (Ptr)(raw + dataOffset) : NULL;
        }
    }

    return &loaded->palette;
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
    if (((TPLLoadedPalette*)pal)->allocationMagic == TPL_ALLOCATION_LOADED ||
        ((TPLFallbackPalette*)pal)->allocationMagic == TPL_ALLOCATION_FALLBACK) {
        return;
    }

    /*
     * The disc header stores a 32-bit descriptor offset in a 12-byte header.
     * It can only be rebound in place when host pointers are also 32-bit.
     * TPLGetPalette uses a separate host-native metadata allocation on 64-bit
     * builds, so never overwrite the first on-disc descriptor with a pointer.
     */
    if (sizeof(void*) > sizeof(u32)) return;

    base = (u8*)pal;
    pal->versionNumber = OSReadBigEndian32(base + 0);
    pal->numDescriptors = OSReadBigEndian32(base + 4);
    desc_off = OSReadBigEndian32(base + 8);

    if (pal->versionNumber != TPL_VERSION) return;
    if (pal->numDescriptors == 0 || pal->numDescriptors > 4096) return;
    if (desc_off == 0) return;

    pal->descriptorArray = (TPLDescriptorPtr)(base + desc_off);
    num = pal->numDescriptors;
    desc_raw = base + desc_off;

    for (i = 0; i < num; ++i) {
        u32 tex_off = OSReadBigEndian32(desc_raw + (i * 8u) + 0);
        u32 clut_off = OSReadBigEndian32(desc_raw + (i * 8u) + 4);
        TPLDescriptor* desc = &pal->descriptorArray[i];

        desc->textureHeader = tex_off ? (TPLHeaderPtr)(base + tex_off) : NULL;
        desc->CLUTHeader = clut_off ? (TPLClutHeaderPtr)(base + clut_off) : NULL;

        if (desc->textureHeader) {
            TPLHeader* tex = desc->textureHeader;
            const u8* raw = base + tex_off;
            u32 data_off = OSReadBigEndian32(raw + 8);

            tex->height = OSReadBigEndian16(raw + 0);
            tex->width = OSReadBigEndian16(raw + 2);
            tex->format = OSReadBigEndian32(raw + 4);
            tex->data = data_off ? (Ptr)(base + data_off) : NULL;
            tex->wrapS = (GXTexWrapMode)OSReadBigEndian32(raw + 12);
            tex->wrapT = (GXTexWrapMode)OSReadBigEndian32(raw + 16);
            tex->minFilter = (GXTexFilter)OSReadBigEndian32(raw + 20);
            tex->magFilter = (GXTexFilter)OSReadBigEndian32(raw + 24);
            tex->LODBias = OSReadBigEndianF32(raw + 28);
            tex->edgeLODEnable = raw[32];
            tex->minLOD = raw[33];
            tex->maxLOD = raw[34];
            tex->unpacked = 1;
        }

        if (desc->CLUTHeader) {
            TPLClutHeader* clut = desc->CLUTHeader;
            const u8* raw = base + clut_off;
            u32 data_off = OSReadBigEndian32(raw + 8);

            clut->numEntries = OSReadBigEndian16(raw + 0);
            clut->unpacked = 1;
            clut->pad8 = raw[3];
            clut->format = (GXTlutFmt)OSReadBigEndian32(raw + 4);
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
    if (!desc || !desc->textureHeader) {
        tpl_init_fallback_texture(to);
        return;
    }
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

    if (!tpl_try_open(name, &dfi)) {
        *pal = tpl_create_fallback_palette();
        return;
    }

    alloc_len = OSRoundUp32B((u32)dfi.length);
    mem = tpl_alloc_aligned(alloc_len);
    if (!mem) {
        DVDClose(&dfi);
        return;
    }
    memset(mem, 0, alloc_len);

    read_len = DVDRead(&dfi, mem, (s32)dfi.length, 0);
    DVDClose(&dfi);
    if (read_len != (s32)dfi.length) {
        tpl_free_aligned(mem);
        return;
    }

    *pal = tpl_parse_loaded_palette(mem, (size_t)dfi.length);
    if (!*pal) {
        tpl_free_aligned(mem);
    }
}

void TPLReleasePalette(TPLPalettePtr* pal) {
    TPLLoadedPalette* loaded;
    if (!pal || !*pal) return;
    loaded = (TPLLoadedPalette*)*pal;
    if (loaded->allocationMagic == TPL_ALLOCATION_LOADED) {
        tpl_free_aligned(loaded->rawData);
    }
    tpl_free_aligned(*pal);
    *pal = NULL;
}
