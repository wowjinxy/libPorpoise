#include <dolphin/gx/GXData.h>
#include <dolphin/hw_regs.h>
#include <dolphin/exi.h>
#include <dolphin/pad.h>
#include <dolphin/tpl.h>
#include <revolution/gx.h>
#include <simulator/sim_gx_CommandProcessor.h>
#include <simulator/sim_gx_Geometry.hpp>
#include <simulator/sim_gx_GlRenderer.hpp>
#include <simulator/sim_gx_State.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string_view>
#include <vector>

extern const char* SIM_GXFragmentShader;

extern "C" u32 VIGetCurrentLine(void) {
    return 0;
}

extern "C" u32 VIGetTvFormat(void) {
    return VI_NTSC;
}

extern "C" void __GXHostRecordPrimitive(
    u32 primitive, u32 vertexCount, u32 textureStages);
extern "C" void __PADHostMergeKeyboardState(
    PADStatus* status, const PADStatus* keyboard);

namespace {

static_assert(sizeof(GXTexObj) == 0x20);
static_assert(sizeof(GXTexObjPriv) == 0x20);
static_assert(sizeof(GXTlutObj) == 0x0c);
static_assert(GX_CC_CPREV == 0);
static_assert(GX_CC_APREV == 1);
static_assert(GX_CC_C0 == 2);
static_assert(GX_CC_A0 == 3);
static_assert(GX_CC_C1 == 4);
static_assert(GX_CC_A1 == 5);
static_assert(GX_CC_C2 == 6);
static_assert(GX_CC_A2 == 7);

bool NearlyEqual(float left, float right, float tolerance = 0.0001f) {
    return std::fabs(left - right) <= tolerance;
}

GXTlutRegion TestTlutRegion = {};

GXTlutRegion* GetTestTlutRegion(u32) {
    return &TestTlutRegion;
}

bool TestTextureCopyIntensityConversion() {
    return
        SIM::GX::ConvertRgbToCopyIntensity(0, 0, 0) == 16 &&
        SIM::GX::ConvertRgbToCopyIntensity(255, 255, 255) == 235 &&
        SIM::GX::ConvertRgbToCopyIntensity(255, 0, 0) == 82 &&
        SIM::GX::ConvertRgbToCopyIntensity(0, 255, 0) == 145 &&
        SIM::GX::ConvertRgbToCopyIntensity(0, 0, 255) == 41;
}

bool TestTextureCopyChannelSelection() {
    return
        SIM::GX::ConvertColorToTextureCopyByte(
            GX_CTF_A8, 10, 20, 30, 40) == 40 &&
        SIM::GX::ConvertColorToTextureCopyByte(
            GX_CTF_R8, 10, 20, 30, 40) == 10 &&
        SIM::GX::ConvertColorToTextureCopyByte(
            GX_CTF_G8, 10, 20, 30, 40) == 20 &&
        SIM::GX::ConvertColorToTextureCopyByte(
            GX_CTF_B8, 10, 20, 30, 40) == 30;
}

bool TestRgb565TextureCopyEncoding() {
    std::array<u8, 4 * 4 * 4> rgba = {};
    rgba[0] = 255;
    rgba[4 + 1] = 255;
    rgba[8 + 2] = 255;
    rgba[12] = 255;
    rgba[12 + 1] = 255;
    rgba[12 + 2] = 255;

    std::array<u8, 4 * 4 * 2> encoded = {};
    SIM::GX::EncodeRgb565TextureCopy(
        rgba.data(), 4, 4, encoded.data());

    return
        encoded[0] == 0xf8 && encoded[1] == 0x00 &&
        encoded[2] == 0x07 && encoded[3] == 0xe0 &&
        encoded[4] == 0x00 && encoded[5] == 0x1f &&
        encoded[6] == 0xff && encoded[7] == 0xff;
}

bool TestDepthTextureCopyEncoding() {
    std::array<float, 4 * 4> depth = {};
    depth[0] = 0.25f;
    depth[1] = 0.5f;

    std::array<u8, 8 * 4> encodedZ8 = {};
    SIM::GX::EncodeDepthTextureCopy(
        depth.data(), 4, 4, GX_TF_Z8, encodedZ8.data());
    if (encodedZ8[0] != 0x80 || encodedZ8[1] != 0xff) {
        return false;
    }

    std::array<u8, 4 * 4 * 2> encodedZ16 = {};
    SIM::GX::EncodeDepthTextureCopy(
        depth.data(), 4, 4, GX_TF_Z16, encodedZ16.data());
    return
        encodedZ16[0] == 0x00 && encodedZ16[1] == 0x80 &&
        encodedZ16[2] == 0xff && encodedZ16[3] == 0xff;
}

bool TestGXCompressZ16Vectors() {
    struct CompressionVector {
        u32 z24;
        std::array<u32, 4> expected;
    };
    constexpr std::array<GXZFmt16, 4> formats = {
        GX_ZC_LINEAR,
        GX_ZC_NEAR,
        GX_ZC_MID,
        GX_ZC_FAR,
    };
    constexpr std::array<CompressionVector, 9> vectors = {{
        {0x000000u, {0x0000u, 0x0000u, 0x0000u, 0x0000u}},
        {0x123456u, {0x1234u, 0x091au, 0x048du, 0x0246u}},
        {0x7fffffu, {0x7fffu, 0x3fffu, 0x1fffu, 0x0fffu}},
        {0x800000u, {0x8000u, 0x4000u, 0x2000u, 0x1000u}},
        {0xc00000u, {0xc000u, 0x8000u, 0x4000u, 0x2000u}},
        {0xf00000u, {0xf000u, 0xe000u, 0x8000u, 0x4000u}},
        {0xff0000u, {0xff00u, 0xfe00u, 0xf000u, 0x8000u}},
        {0xffff00u, {0xffffu, 0xfffeu, 0xfff0u, 0xcf00u}},
        {0xffffffu, {0xffffu, 0xffffu, 0xffffu, 0xcfffu}},
    }};

    for (const CompressionVector& vector : vectors) {
        for (size_t format = 0; format < formats.size(); ++format) {
            if (GXCompressZ16(vector.z24, formats[format]) !=
                vector.expected[format]) {
                return false;
            }
        }
    }
    return true;
}

bool TestIa8TlutTypedU16Dispatch() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    // SDK code constructs an IA8 entry numerically as I | (A << 8). On a
    // little-endian host the standard GXInitTlutObj call must recognize this
    // typed u16 source and canonicalize it to the GameCube's [A, I] bytes.
    alignas(32) std::array<u16, 2> nativePalette = {
        0x20e0u,
        0x8010u,
    };
    GXInitTlutRegion(&TestTlutRegion, 0x80000u, GX_TLUT_256);
    const GXTlutRegionCallback previousTlutCallback =
        GXSetTlutRegionCallback(GetTestTlutRegion);
    GXTlutObj tlutObject = {};
    GXInitTlutObj(
        &tlutObject,
        nativePalette.data(),
        GX_TL_IA8,
        static_cast<u16>(nativePalette.size()));
    GXLoadTlut(&tlutObject, 6);
    GXSetTlutRegionCallback(previousTlutCallback);

    const auto& tlut = SIM::GX::GetGlobalState().GetTlutState(6);
    const std::array<u8, 4> expectedCanonical = {
        0x20u, 0xe0u, 0x80u, 0x10u,
    };
    if (tlut.canonicalBytes != std::vector<u8>(
            expectedCanonical.begin(), expectedCanonical.end())) {
        return false;
    }

    const SIM::GX::DecodedTlutColor first =
        SIM::GX::DecodeTlutEntry(GX_TL_IA8, tlut.canonicalBytes.data());
    const SIM::GX::DecodedTlutColor second =
        SIM::GX::DecodeTlutEntry(
            GX_TL_IA8, tlut.canonicalBytes.data() + 2);
    return
        first.red == 0xe0u && first.green == 0xe0u &&
        first.blue == 0xe0u && first.alpha == 0x20u &&
        second.red == 0x10u && second.green == 0x10u &&
        second.blue == 0x10u && second.alpha == 0x80u;
}

bool TestPreloadEntireMipChain() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    alignas(32) std::array<u8, 128> image = {};
    GXTexObj texture = {};
    GXTexRegion region = {};
    GXInitTexObj(
        &texture,
        image.data(),
        8,
        4,
        GX_TF_I4,
        GX_CLAMP,
        GX_CLAMP,
        GX_TRUE);
    GXInitTexPreLoadRegion(
        &region,
        0,
        0x80000u,
        0x80000u,
        0x80000u);

    const u32 previousDirtyState = gx->dirtyState;
    const u32 previousBpMask = gx->bpMask;
    const u8 previousDlSaveContext = gx->dlSaveContext;
    const u8 previousBpSent = gx->bpSent;
    gx->dirtyState = 0;
    gx->bpMask = 0xfe000000u;
    gx->dlSaveContext = 0;

    alignas(32) std::array<u8, 128> commands = {};
    GXBeginDisplayList(commands.data(), static_cast<u32>(commands.size()));
    GXPreLoadEntireTexture(&texture, &region);
    const u32 recordedSize = GXEndDisplayList();

    gx->dirtyState = previousDirtyState;
    gx->bpMask = previousBpMask;
    gx->dlSaveContext = previousDlSaveContext;
    gx->bpSent = previousBpSent;

    // There are two BP-mask flushes, four base-level image commands, and
    // four commands for each of the three lower mip levels. The 90 command
    // bytes are padded to the SDK-mandated 32-byte display-list boundary.
    if (recordedSize != 96u) {
        return false;
    }
    size_t commandBytes = 0;
    size_t loadImage3Count = 0;
    while (commandBytes + 5u <= recordedSize &&
           commands[commandBytes] == 0x61u) {
        if (commands[commandBytes + 1u] == 0x63u) {
            ++loadImage3Count;
        }
        commandBytes += 5u;
    }
    if (commandBytes != 90u || loadImage3Count != 4u) {
        return false;
    }
    for (size_t offset = commandBytes; offset < recordedSize; ++offset) {
        if (commands[offset] != 0u) {
            return false;
        }
    }
    return true;
}

bool TestNativeU16TextureSourceEncoding() {
    using SourceEncoding =
        SIM::GX::TextureState::SourceEncoding;

    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    // One 8x8 CMPR tile contains four 4x4 sub-blocks. Each sub-block uses a
    // native RGB565 red endpoint followed by green and selects endpoint zero
    // for every pixel. Reading the little-endian u16 storage as canonical
    // bytes would turn F800 into 00F8 instead of red.
    alignas(32) std::array<u16, 16> nativeCmpr = {};
    for (size_t subBlock = 0; subBlock < 4u; ++subBlock) {
        nativeCmpr[subBlock * 4u] = 0xf800u;
        nativeCmpr[subBlock * 4u + 1u] = 0x07e0u;
    }

    GXTexRegion region = {};
    GXInitTexPreLoadRegion(
        &region,
        0,
        0x80000u,
        0x80000u,
        0x80000u);

    GXTexObj typedObject = {};
    GXInitTexObj(
        &typedObject,
        nativeCmpr.data(),
        8,
        8,
        GX_TF_CMPR,
        GX_CLAMP,
        GX_CLAMP,
        GX_FALSE);
    GXLoadTexObjPreLoaded(&typedObject, &region, GX_TEXMAP0);

    const auto& state = SIM::GX::GetGlobalState();
    const auto& nativeTexture = state.GetTextureState(0);
    if (nativeTexture.data != nativeCmpr.data() ||
        nativeTexture.sourceEncoding != SourceEncoding::NativeU16) {
        return false;
    }

    std::vector<u8> rgba;
    if (!SIM::GX::Detail::DecodeTextureToRgba(
            nativeTexture, {}, rgba) ||
        rgba.size() != 8u * 8u * 4u ||
        rgba[0] != 255u || rgba[1] != 0u ||
        rgba[2] != 0u || rgba[3] != 255u) {
        return false;
    }

    SIM::GX::TextureContentSnapshot snapshot;
    snapshot.Capture(nativeTexture);
    if (!snapshot.Matches(nativeTexture)) {
        return false;
    }

    const u64 textureRevision = nativeTexture.revision;
    const u64 invalidationRevision =
        state.GetTextureInvalidationRevision();
    nativeCmpr[0] = 0x001fu;
    if (SIM::GX::Detail::ShouldValidateTexture(
            false,
            false,
            textureRevision,
            nativeTexture.revision,
            invalidationRevision,
            state.GetTextureInvalidationRevision(),
            0,
            0)) {
        return false;
    }

    GXInvalidateTexAll();
    if (!SIM::GX::Detail::ShouldValidateTexture(
            false,
            false,
            textureRevision,
            nativeTexture.revision,
            invalidationRevision,
            state.GetTextureInvalidationRevision(),
            0,
            0) ||
        snapshot.Matches(nativeTexture) ||
        !SIM::GX::Detail::DecodeTextureToRgba(
            nativeTexture, {}, rgba) ||
        rgba[0] != 0u || rgba[1] != 0u || rgba[2] != 255u) {
        return false;
    }

    // Byte-oriented and pointer-erased standard calls stay canonical.
    alignas(32) std::array<u8, 32> canonicalCmpr = {};
    for (size_t subBlock = 0; subBlock < 4u; ++subBlock) {
        canonicalCmpr[subBlock * 8u] = 0xf8u;
        canonicalCmpr[subBlock * 8u + 2u] = 0x07u;
        canonicalCmpr[subBlock * 8u + 3u] = 0xe0u;
    }
    GXTexObj byteObject = {};
    GXInitTexObj(
        &byteObject,
        canonicalCmpr.data(),
        8,
        8,
        GX_TF_CMPR,
        GX_CLAMP,
        GX_CLAMP,
        GX_FALSE);
    GXLoadTexObjPreLoaded(&byteObject, &region, GX_TEXMAP1);
    if (state.GetTextureState(1).sourceEncoding !=
        SourceEncoding::CanonicalBigEndian) {
        return false;
    }

    void* erasedCanonical = canonicalCmpr.data();
    GXTexObj erasedCanonicalObject = {};
    GXInitTexObj(
        &erasedCanonicalObject,
        erasedCanonical,
        8,
        8,
        GX_TF_CMPR,
        GX_CLAMP,
        GX_CLAMP,
        GX_FALSE);
    GXLoadTexObjPreLoaded(
        &erasedCanonicalObject, &region, GX_TEXMAP2);
    if (state.GetTextureState(2).sourceEncoding !=
        SourceEncoding::CanonicalBigEndian) {
        return false;
    }

    TPLHeader tplHeader = {};
    tplHeader.height = 8;
    tplHeader.width = 8;
    tplHeader.format = GX_TF_CMPR;
    tplHeader.data = reinterpret_cast<Ptr>(canonicalCmpr.data());
    tplHeader.wrapS = GX_CLAMP;
    tplHeader.wrapT = GX_CLAMP;
    tplHeader.minFilter = GX_NEAR;
    tplHeader.magFilter = GX_NEAR;
    TPLDescriptor tplDescriptor = {&tplHeader, nullptr};
    TPLPalette tplPalette = {0x0020af30u, 1, &tplDescriptor};
    GXTexObj tplObject = {};
    TPLGetGXTexObjFromPalette(&tplPalette, &tplObject, 0);
    GXLoadTexObjPreLoaded(&tplObject, &region, GX_TEXMAP4);
    if (state.GetTextureState(4).sourceEncoding !=
        SourceEncoding::CanonicalBigEndian) {
        return false;
    }

    // Once middleware has erased the u16 type, the explicit API still
    // records the intended scalar-word encoding without relying on dispatch.
    void* erasedNative = nativeCmpr.data();
    GXTexObj explicitObject = {};
    GXInitTexObjHostNativeU16(
        &explicitObject,
        erasedNative,
        8,
        8,
        GX_TF_CMPR,
        GX_CLAMP,
        GX_CLAMP,
        GX_FALSE);
    GXLoadTexObjPreLoaded(&explicitObject, &region, GX_TEXMAP3);
    return
        state.GetTextureState(3).data == erasedNative &&
        state.GetTextureState(3).sourceEncoding ==
            SourceEncoding::NativeU16;
}

bool TestMipPayloadLayoutDecodeAndFiltering() {
    using SIM::GX::Detail::TextureMipmapFilter;
    using SIM::GX::TextureMipLevelLayout;

    SIM::GX::TextureState texture;
    texture.width = 8;
    texture.height = 8;
    texture.format = GX_TF_CMPR;
    texture.mipmap = true;
    texture.maxLod = 3.0f;

    constexpr std::array<u16, 4> endpoints = {
        0xf800u,
        0x07e0u,
        0x001fu,
        0xffffu,
    };
    constexpr std::array<std::array<u8, 4>, 4> expectedColors = {{
        {255u, 0u, 0u, 255u},
        {0u, 255u, 0u, 255u},
        {0u, 0u, 255u, 255u},
        {255u, 255u, 255u, 255u},
    }};
    alignas(32) std::array<u8, 128> canonical = {};
    for (size_t level = 0u; level < endpoints.size(); ++level) {
        for (size_t subBlock = 0u; subBlock < 4u; ++subBlock) {
            const size_t offset = level * 32u + subBlock * 8u;
            canonical[offset] = static_cast<u8>(endpoints[level] >> 8u);
            canonical[offset + 1u] = static_cast<u8>(endpoints[level]);
        }
    }
    texture.data = canonical.data();

    if (GXGetTexBufferSize(8, 8, GX_TF_CMPR, GX_TRUE, 3) != 96u ||
        GXGetTexBufferSize(8, 8, GX_TF_CMPR, GX_TRUE, 4) != 128u ||
        SIM::GX::GetTextureMipLevelCount(texture) != 4u ||
        SIM::GX::GetTextureSourceByteSize(texture) != canonical.size()) {
        return false;
    }

    constexpr std::array<u16, 4> expectedWidths = {8, 4, 2, 1};
    for (size_t level = 0u; level < endpoints.size(); ++level) {
        TextureMipLevelLayout layout;
        std::vector<u8> rgba;
        if (!SIM::GX::GetTextureMipLevelLayout(texture, level, layout) ||
            layout.offset != level * 32u || layout.byteSize != 32u ||
            layout.width != expectedWidths[level] ||
            layout.height != expectedWidths[level] ||
            !SIM::GX::Detail::DecodeCanonicalTextureMipLevelToRgba(
                texture,
                canonical.data(),
                canonical.size(),
                level,
                {},
                rgba) ||
            rgba.size() != static_cast<size_t>(layout.width) *
                               static_cast<size_t>(layout.height) * 4u ||
            !std::equal(
                expectedColors[level].begin(),
                expectedColors[level].end(),
                rgba.begin())) {
            return false;
        }
    }

    alignas(32) std::array<u16, 64> nativeWords = {};
    for (size_t word = 0u; word < nativeWords.size(); ++word) {
        nativeWords[word] = static_cast<u16>(
            (static_cast<u16>(canonical[word * 2u]) << 8u) |
            canonical[word * 2u + 1u]);
    }
    texture.data = nativeWords.data();
    texture.sourceEncoding =
        SIM::GX::TextureState::SourceEncoding::NativeU16;
    std::vector<u8> recanonicalized;
    if (!SIM::GX::CopyCanonicalTextureBytes(texture, recanonicalized) ||
        recanonicalized != std::vector<u8>(
            canonical.begin(), canonical.end())) {
        return false;
    }
    SIM::GX::TextureContentSnapshot snapshot;
    snapshot.Capture(texture);
    nativeWords[48] = 0xf800u;
    if (snapshot.Matches(texture)) {
        return false;
    }

    struct FilterCase {
        GXTexFilter filter;
        bool linearTexels;
        TextureMipmapFilter mipmapFilter;
    };
    constexpr std::array<FilterCase, 6> filterCases = {{
        {GX_NEAR, false, TextureMipmapFilter::None},
        {GX_LINEAR, true, TextureMipmapFilter::None},
        {GX_NEAR_MIP_NEAR, false, TextureMipmapFilter::Nearest},
        {GX_LIN_MIP_NEAR, true, TextureMipmapFilter::Nearest},
        {GX_NEAR_MIP_LIN, false, TextureMipmapFilter::Linear},
        {GX_LIN_MIP_LIN, true, TextureMipmapFilter::Linear},
    }};
    for (const FilterCase& filterCase : filterCases) {
        const auto selection =
            SIM::GX::Detail::SelectTextureFilter(filterCase.filter);
        if (selection.linearTexels != filterCase.linearTexels ||
            selection.mipmapFilter != filterCase.mipmapFilter) {
            return false;
        }
    }
    return true;
}

bool TestNormalMatrixState() {
    SIM::GX::GlobalState state;
    const std::array<float, 9> matrix = {
        0.25f, 0.0f, 0.0f,
        0.0f, 0.5f, 0.0f,
        0.0f, 0.0f, 2.0f,
    };
    state.SetXfData(
        0x400,
        reinterpret_cast<const u8*>(matrix.data()),
        matrix.size());

    const auto& decoded = state.GetNormalMatrix(0);
    return
        decoded[0] == 0.25f &&
        decoded[5] == 0.5f &&
        decoded[10] == 2.0f &&
        decoded[15] == 1.0f;
}

bool TestIndependentTextureLodSetters() {
    GXTexObj texture = {};

    GXInitTexObjMaxLOD(&texture, 12.0f);
    GXInitTexObjMinLOD(&texture, -1.0f);
    GXInitTexObjLODBias(&texture, -1.5f);
    GXInitTexObjBiasClamp(&texture, GX_TRUE);
    GXInitTexObjEdgeLOD(&texture, GX_TRUE);
    GXInitTexObjMaxAniso(&texture, GX_ANISO_4);

    return
        NearlyEqual(GXGetTexObjMaxLOD(&texture), 10.0f) &&
        NearlyEqual(GXGetTexObjMinLOD(&texture), 0.0f) &&
        NearlyEqual(GXGetTexObjLODBias(&texture), -1.5f) &&
        GXGetTexObjBiasClamp(&texture) == GX_TRUE &&
        GXGetTexObjEdgeLOD(&texture) == GX_TRUE &&
        GXGetTexObjMaxAniso(&texture) == GX_ANISO_4;
}

bool TestHostTexturePointerPreserved() {
    alignas(32) std::array<u8, 32> image = {};
    GXTexObj texture = {};
    int userData = 42;

    GXInitTexObj(
        &texture,
        image.data(),
        8,
        8,
        GX_TF_I4,
        GX_CLAMP,
        GX_CLAMP,
        GX_FALSE);
    GXInitTexObjUserData(&texture, &userData);

    void* decodedImage = nullptr;
    u16 width = 0;
    u16 height = 0;
    GXTexFmt format = GX_TF_I8;
    GXTexWrapMode wrapS = GX_REPEAT;
    GXTexWrapMode wrapT = GX_REPEAT;
    GXBool mipmap = GX_TRUE;
    GXGetTexObjAll(
        &texture,
        &decodedImage,
        &width,
        &height,
        &format,
        &wrapS,
        &wrapT,
        &mipmap);

    return
        GXGetTexObjData(&texture) == image.data() &&
        GXGetTexObjUserData(&texture) == &userData &&
        decodedImage == image.data() &&
        width == 8 &&
        height == 8 &&
        format == GX_TF_I4 &&
        wrapS == GX_CLAMP &&
        wrapT == GX_CLAMP &&
        mipmap == GX_FALSE;
}

bool TestTextureObjectAbiBounds() {
    struct GuardedTexture {
        GXTexObj texture;
        std::array<u8, 16> guard;
    } guardedTexture = {};
    struct GuardedTlut {
        GXTlutObj tlut;
        std::array<u8, 16> guard;
    } guardedTlut = {};
    alignas(32) std::array<u8, 32> image = {};
    alignas(32) std::array<u16, 17> palette = {};

    guardedTexture.guard.fill(0xa5);
    guardedTlut.guard.fill(0x5a);

    GXInitTexObj(
        &guardedTexture.texture,
        image.data(),
        8,
        8,
        GX_TF_I4,
        GX_CLAMP,
        GX_CLAMP,
        GX_FALSE);
    GXInitTexObjLOD(
        &guardedTexture.texture,
        GX_NEAR,
        GX_LINEAR,
        0.0f,
        0.0f,
        0.0f,
        GX_FALSE,
        GX_FALSE,
        GX_ANISO_1);
    GXInitTlutObj(
        &guardedTlut.tlut,
        palette.data(),
        GX_TL_IA8,
        16);

    for (u8 value : guardedTexture.guard) {
        if (value != 0xa5) {
            return false;
        }
    }
    for (u8 value : guardedTlut.guard) {
        if (value != 0x5a) {
            return false;
        }
    }
    if (GXGetTlutObjData(&guardedTlut.tlut) != palette.data()) {
        return false;
    }

#ifdef LIBPORPOISE_PORT
    // Native host palettes are copied synchronously at GXLoadTlut and do not
    // inherit the hardware DMA source's 32-byte alignment requirement.
    GXInitTlutObjHostNativeU16(
        &guardedTlut.tlut,
        palette.data() + 1,
        GX_TL_RGB5A3,
        16);
    if (GXGetTlutObjData(&guardedTlut.tlut) != palette.data() + 1) {
        return false;
    }
#endif
    for (u8 value : guardedTlut.guard) {
        if (value != 0x5a) {
            return false;
        }
    }
    return true;
}

bool TestSdkTextureLodEncoding() {
    alignas(32) std::array<u8, 128> image = {};
    GXTexObj directTexture = {};
    GXInitTexObj(
        &directTexture,
        image.data(),
        8,
        4,
        GX_TF_I4,
        GX_CLAMP,
        GX_CLAMP,
        GX_TRUE);
    if (!NearlyEqual(GXGetTexObjMaxLOD(&directTexture), 3.0f)) {
        return false;
    }

    TPLHeader header = {};
    header.height = 8;
    header.width = 8;
    header.format = GX_TF_I4;
    header.data = reinterpret_cast<Ptr>(image.data());
    header.wrapS = GX_CLAMP;
    header.wrapT = GX_CLAMP;
    header.minFilter = GX_NEAR_MIP_NEAR;
    header.magFilter = GX_LINEAR;
    header.LODBias = 0.5f;
    header.edgeLODEnable = GX_TRUE;
    header.minLOD = 2;
    header.maxLOD = 5;
    TPLDescriptor descriptor = {&header, nullptr};
    TPLPalette palette = {0x0020af30u, 1, &descriptor};
    GXTexObj tplTexture = {};
    TPLGetGXTexObjFromPalette(&palette, &tplTexture, 0);

    if (!NearlyEqual(GXGetTexObjMinLOD(&tplTexture), 2.0f) ||
        !NearlyEqual(GXGetTexObjMaxLOD(&tplTexture), 5.0f) ||
        !NearlyEqual(GXGetTexObjLODBias(&tplTexture), 0.5f) ||
        GXGetTexObjEdgeLOD(&tplTexture) != GX_TRUE) {
        return false;
    }

    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();
    GXTexRegion region = {};
    GXInitTexPreLoadRegion(
        &region,
        0,
        0x80000u,
        0x80000u,
        0x80000u);
    GXLoadTexObjPreLoaded(&tplTexture, &region, GX_TEXMAP7);
    const auto& loaded =
        SIM::GX::GetGlobalState().GetTextureState(7);
    return
        loaded.mipmap &&
        loaded.minFilter == GX_NEAR_MIP_NEAR &&
        loaded.magFilter == GX_LINEAR &&
        NearlyEqual(loaded.minLod, 2.0f) &&
        NearlyEqual(loaded.maxLod, 5.0f) &&
        NearlyEqual(loaded.lodBias, 0.5f) &&
        SIM::GX::GetTextureMipLevelCount(loaded) == 4u &&
        SIM::GX::GetTextureSourceByteSize(loaded) == image.size();
}

bool TestHostExiSyncCompletion() {
    if (!EXISelect(2, 0, EXI_FREQ_1M)) {
        return false;
    }

    std::array<u8, 2> command = {0x12u, 0x34u};
    if (!EXIImm(2, command.data(), 2, EXI_WRITE, nullptr) ||
        (EXIGetState(2) & EXI_STATE_BUSY) == 0u ||
        EXIREG(2, 4) != 0x12340000u ||
        !EXISync(2) ||
        (EXIGetState(2) & EXI_STATE_BUSY) != 0u ||
        (EXIREG(2, 3) & 1u) != 0u) {
        return false;
    }

    std::array<u8, 4> response = {};
    if (!EXIImm(2, response.data(), 4, EXI_READ, nullptr)) {
        return false;
    }
    EXIREG(2, 4) = 0x89abcdefu;
    if (!EXISync(2) ||
        response != std::array<u8, 4>{0x89u, 0xabu, 0xcdu, 0xefu} ||
        (EXIGetState(2) & EXI_STATE_BUSY) != 0u ||
        (EXIREG(2, 3) & 1u) != 0u) {
        return false;
    }
    return EXIDeselect(2) == TRUE;
}

bool TestFifoQueries() {
    GXFifoObj fifo = {};
    auto* state = reinterpret_cast<__GXFifoObj*>(&fifo);
    state->count = 0x1234;
    state->wrap = GX_TRUE;

    return GXGetFifoCount(&fifo) == 0x1234 &&
           GXGetFifoWrap(&fifo) == GX_TRUE;
}

u16 DrawSyncCallbackToken = 0;
u32 DrawSyncCallbackCount = 0;

void RecordDrawSyncToken(u16 token) {
    DrawSyncCallbackToken = token;
    DrawSyncCallbackCount++;
}

bool TestHostDrawSyncCompletion() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    DrawSyncCallbackToken = 0;
    DrawSyncCallbackCount = 0;
    const GXDrawSyncCallback previousCallback =
        GXSetDrawSyncCallback(RecordDrawSyncToken);

    GXSetDrawSync(0xb00b);
    const bool firstTokenCompleted =
        GXReadDrawSync() == 0xb00b &&
        DrawSyncCallbackToken == 0xb00b &&
        DrawSyncCallbackCount == 1;

    GXSetDrawSync(0xbeef);
    const bool secondTokenCompleted =
        GXReadDrawSync() == 0xbeef &&
        DrawSyncCallbackToken == 0xbeef &&
        DrawSyncCallbackCount == 2;

    GXSetDrawSyncCallback(previousCallback);
    return firstTokenCompleted && secondTokenCompleted;
}

bool TestHostPadKeyboardFallback() {
    std::array<PADStatus, PAD_MAX_CONTROLLERS> pads = {};
    if (!PADInit()) {
        return false;
    }
    PADRead(pads.data());
    return pads[0].err == PAD_ERR_NONE;
}

bool TestHostPadKeyboardMerge() {
    PADStatus controller = {};
    controller.button = PAD_BUTTON_A | PAD_BUTTON_X;
    controller.stickX = 17;
    controller.stickY = -31;
    controller.substickX = 11;
    controller.substickY = -9;
    controller.triggerLeft = 100;
    controller.triggerRight = 200;
    controller.analogA = 64;
    controller.analogB = 220;
    controller.err = PAD_ERR_TRANSFER;

    PADStatus keyboard = {};
    keyboard.button = PAD_BUTTON_B | PAD_BUTTON_START | PAD_TRIGGER_L;
    keyboard.stickY = 100;
    keyboard.substickX = -100;
    keyboard.triggerLeft = 255;
    keyboard.triggerRight = 10;
    keyboard.analogA = 255;
    keyboard.analogB = 50;
    keyboard.err = PAD_ERR_NONE;

    __PADHostMergeKeyboardState(&controller, &keyboard);

    return
        controller.button ==
            (PAD_BUTTON_A | PAD_BUTTON_B | PAD_BUTTON_X |
             PAD_BUTTON_START | PAD_TRIGGER_L) &&
        controller.stickX == 17 &&
        controller.stickY == 100 &&
        controller.substickX == -100 &&
        controller.substickY == -9 &&
        controller.triggerLeft == 255 &&
        controller.triggerRight == 200 &&
        controller.analogA == 255 &&
        controller.analogB == 220 &&
        controller.err == PAD_ERR_TRANSFER;
}

bool TestZScaleOffsetCommands() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    GXSetZScaleOffset(0.5f, -0.25f);
    GXSetViewport(10.0f, 20.0f, 640.0f, 480.0f, 0.25f, 0.75f);

    const auto& state = SIM::GX::GetGlobalState();
    if (!state.HasViewportTransform()) {
        return false;
    }

    const auto& viewport = state.GetViewportTransform();
    const auto& logicalViewport = state.GetViewportState();
    const float expectedScaleZ =
        (gx->vpFarz - gx->vpNearz) * gx->zScale;
    const float expectedOffsetZ =
        gx->vpFarz * gx->zScale + gx->zOffset;

    return
        NearlyEqual(viewport[0], 320.0f) &&
        NearlyEqual(viewport[1], -240.0f) &&
        NearlyEqual(viewport[2], expectedScaleZ, 1.0f) &&
        NearlyEqual(viewport[3], 672.0f) &&
        NearlyEqual(viewport[4], 602.0f) &&
        NearlyEqual(viewport[5], expectedOffsetZ, 1.0f) &&
        logicalViewport.valid &&
        NearlyEqual(logicalViewport.left, 10.0f) &&
        NearlyEqual(logicalViewport.top, 20.0f) &&
        NearlyEqual(logicalViewport.width, 640.0f) &&
        NearlyEqual(logicalViewport.height, 480.0f);
}

bool TestPositionTextureCoordinateGeneration() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    Mtx textureMatrix = {
        {1.0f, 2.0f, 3.0f, 4.0f},
        {5.0f, 6.0f, 7.0f, 8.0f},
        {9.0f, 10.0f, 11.0f, 12.0f},
    };
    GXLoadTexMtxImm(textureMatrix, GX_TEXMTX3, GX_MTX3x4);
    GXSetTexCoordGen(
        GX_TEXCOORD0,
        GX_TG_MTX3x4,
        GX_TG_POS,
        GX_TEXMTX3);

    const auto& state = SIM::GX::GetGlobalState();
    const auto& texGen = state.GetTexCoordGenState(0);
    const auto& decodedMatrix = state.GetTexCoordGenMatrix(0);
    return
        texGen.source == GX_TG_POS &&
        texGen.matrixId == GX_TEXMTX3 &&
        NearlyEqual(decodedMatrix[0], 1.0f) &&
        NearlyEqual(decodedMatrix[3], 4.0f) &&
        NearlyEqual(decodedMatrix[4], 5.0f) &&
        NearlyEqual(decodedMatrix[7], 8.0f) &&
        NearlyEqual(decodedMatrix[8], 9.0f) &&
        NearlyEqual(decodedMatrix[11], 12.0f);
}

bool TestTextureMatrix2x4ImplicitQ() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    Mtx textureMatrix = {
        {0.5f, 0.0f, 0.0f, 0.25f},
        {0.0f, 0.5f, 0.0f, 0.50f},
        {9.0f, 10.0f, 11.0f, 12.0f},
    };
    GXLoadTexMtxImm(textureMatrix, GX_TEXMTX0, GX_MTX2x4);
    GXSetTexCoordGen(
        GX_TEXCOORD0,
        GX_TG_MTX2x4,
        GX_TG_TEX0,
        GX_TEXMTX0);

    const auto& matrix =
        SIM::GX::GetGlobalState().GetTexCoordGenMatrix(0);
    return
        NearlyEqual(matrix[0], 0.5f) &&
        NearlyEqual(matrix[3], 0.25f) &&
        NearlyEqual(matrix[5], 0.5f) &&
        NearlyEqual(matrix[7], 0.50f) &&
        NearlyEqual(matrix[8], 0.0f) &&
        NearlyEqual(matrix[9], 0.0f) &&
        NearlyEqual(matrix[10], 0.0f) &&
        NearlyEqual(matrix[11], 1.0f);
}

bool TestTextureCoordinateSourceImplicitQ() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    GXSetTexCoordGen(
        GX_TEXCOORD0,
        GX_TG_MTX3x4,
        GX_TG_TEX0,
        GX_IDENTITY);

    SIM::GX::RenderVertex vertex;
    vertex.texCoords[0] = {0.25f, 0.75f, 1.0f};
    std::vector<SIM::GX::RenderVertex> vertices = {vertex};
    SIM::GX::ApplyTextureCoordinateGeneration(
        SIM::GX::GetGlobalState(),
        vertices);

    return
        NearlyEqual(vertices[0].texCoords[0].s, 0.25f) &&
        NearlyEqual(vertices[0].texCoords[0].t, 0.75f) &&
        NearlyEqual(vertices[0].texCoords[0].q, 1.0f);
}

bool TestDualTextureCoordinateGeneration() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    Mtx positionMatrix = {
        {2.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 3.0f, 0.0f, 2.0f},
        {0.0f, 0.0f, 4.0f, 3.0f},
    };
    Mtx postMatrix = {
        {0.5f, 0.0f, 0.0f, 0.25f},
        {0.0f, 0.25f, 0.0f, 0.50f},
        {0.0f, 0.0f, 2.0f, 1.0f},
    };
    GXLoadPosMtxImm(positionMatrix, GX_PNMTX0);
    GXLoadTexMtxImm(postMatrix, GX_PTTEXMTX0, GX_MTX3x4);
    GXSetTexCoordGen2(
        GX_TEXCOORD0,
        GX_TG_MTX3x4,
        GX_TG_POS,
        GX_PNMTX0,
        GX_TRUE,
        GX_PTTEXMTX0);

    SIM::GX::RenderVertex vertex;
    vertex.position = {1.0f, 1.0f, 1.0f};
    std::vector<SIM::GX::RenderVertex> vertices = {vertex};
    SIM::GX::ApplyTextureCoordinateGeneration(
        SIM::GX::GetGlobalState(),
        vertices);

    const float primaryLength = std::sqrt(83.0f);
    const auto& texGen =
        SIM::GX::GetGlobalState().GetTexCoordGenState(0);
    return
        texGen.matrixId == GX_PNMTX0 &&
        texGen.postMatrixId == GX_PTTEXMTX0 &&
        texGen.normalize &&
        NearlyEqual(
            vertices[0].texCoords[0].s,
            0.5f * 3.0f / primaryLength + 0.25f) &&
        NearlyEqual(
            vertices[0].texCoords[0].t,
            0.25f * 5.0f / primaryLength + 0.50f) &&
        NearlyEqual(
            vertices[0].texCoords[0].q,
            2.0f * 7.0f / primaryLength + 1.0f);
}

bool TestHostPerformanceMetrics() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();
    __cpReg = __CPRegs;
    __peReg = __PERegs;
    __memReg = __MEMRegs;

    GXClearMemMetric();
    GXSetGPMetric(GX_PERF0_CLOCKS, GX_PERF1_VERTICES);
    GXClearGPMetric();
    __GXHostRecordPrimitive(GX_TRIANGLES, 6, 1);

    u32 clocks = 0;
    u32 vertices = 0;
    GXReadGPMetric(&clocks, &vertices);

    u32 topIn = 0;
    u32 topOut = 0;
    u32 bottomIn = 0;
    u32 bottomOut = 0;
    u32 colorIn = 0;
    u32 copyClocks = 0;
    GXReadPixMetric(
        &topIn,
        &topOut,
        &bottomIn,
        &bottomOut,
        &colorIn,
        &copyClocks);

    u32 cpRequests = 0;
    u32 textureRequests = 0;
    u32 unused[8] = {};
    GXReadMemMetric(
        &cpRequests,
        &textureRequests,
        &unused[0],
        &unused[1],
        &unused[2],
        &unused[3],
        &unused[4],
        &unused[5],
        &unused[6],
        &unused[7]);

    GXClearPixMetric();
    u32 beforeQueuedReset = 0;
    GXReadPixMetric(
        &beforeQueuedReset,
        &topOut,
        &bottomIn,
        &bottomOut,
        &colorIn,
        &copyClocks);
    __GXHostRecordPrimitive(GX_TRIANGLES, 3, 1);
    u32 afterQueuedReset = 0;
    GXReadPixMetric(
        &afterQueuedReset,
        &topOut,
        &bottomIn,
        &bottomOut,
        &colorIn,
        &copyClocks);

    return
        clocks > 0 &&
        vertices == 6 &&
        topIn > 0 &&
        topOut == afterQueuedReset &&
        textureRequests > 0 &&
        cpRequests > 0 &&
        beforeQueuedReset == topIn &&
        afterQueuedReset > 0 &&
        afterQueuedReset < beforeQueuedReset;
}

bool TestResetWritePipeCompatibility() {
    GXSetResetWritePipe(GX_TRUE);
    GXSetResetWritePipe(GX_FALSE);
    return true;
}

bool TestXfChannelAndLightState() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    const GXColor ambient = {16, 32, 48, 64};
    const GXColor material = {192, 128, 64, 255};
    const GXColor lightColor = {255, 224, 192, 160};
    GXLightObj light = {};

    GXSetChanAmbColor(GX_COLOR0A0, ambient);
    GXSetChanMatColor(GX_COLOR0A0, material);
    GXSetChanCtrl(
        GX_COLOR0A0,
        GX_ENABLE,
        GX_SRC_REG,
        GX_SRC_REG,
        GX_LIGHT0 | GX_LIGHT5,
        GX_DF_CLAMP,
        GX_AF_NONE);
    GXInitLightColor(&light, lightColor);
    GXInitLightSpot(&light, 0.0f, GX_SP_OFF);
    GXInitLightDistAttn(&light, 0.0f, 0.0f, GX_DA_OFF);
    GXInitLightPos(&light, 1.0f, 2.0f, 3.0f);
    GXLoadLightObjImm(&light, GX_LIGHT0);

    const auto& state = SIM::GX::GetGlobalState();
    const auto& channel = state.GetChannelState(0);
    const auto& loadedLight = state.GetLightState(0);
    return
        NearlyEqual(channel.ambientColor[0], 16.0f / 255.0f) &&
        NearlyEqual(channel.ambientColor[1], 32.0f / 255.0f) &&
        NearlyEqual(channel.ambientColor[2], 48.0f / 255.0f) &&
        NearlyEqual(channel.ambientColor[3], 64.0f / 255.0f) &&
        NearlyEqual(channel.materialColor[0], 192.0f / 255.0f) &&
        NearlyEqual(channel.materialColor[1], 128.0f / 255.0f) &&
        NearlyEqual(channel.materialColor[2], 64.0f / 255.0f) &&
        NearlyEqual(channel.materialColor[3], 1.0f) &&
        channel.colorControl.lightingEnabled &&
        channel.colorControl.materialSource == GX_SRC_REG &&
        channel.colorControl.ambientSource == GX_SRC_REG &&
        channel.colorControl.lightMask == (GX_LIGHT0 | GX_LIGHT5) &&
        channel.colorControl.diffuseFunction == GX_DF_CLAMP &&
        channel.colorControl.attenuationFunction == GX_AF_NONE &&
        loadedLight.valid &&
        NearlyEqual(loadedLight.color[0], 1.0f) &&
        NearlyEqual(loadedLight.color[1], 224.0f / 255.0f) &&
        NearlyEqual(loadedLight.color[2], 192.0f / 255.0f) &&
        NearlyEqual(loadedLight.color[3], 160.0f / 255.0f) &&
        NearlyEqual(loadedLight.cosineAttenuation[0], 1.0f) &&
        NearlyEqual(loadedLight.distanceAttenuation[0], 1.0f) &&
        NearlyEqual(loadedLight.position[0], 1.0f) &&
        NearlyEqual(loadedLight.position[1], 2.0f) &&
        NearlyEqual(loadedLight.position[2], 3.0f);
}

void SendBpRegister(u32 value) {
    SIM_GX_CommandProcessor_SendU8(0x61);
    SIM_GX_CommandProcessor_SendU32(value);
}

bool TestBpRenderStateCommands() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    const u32 blendValue =
        0x41000000u |
        (1u << 0) |
        (1u << 2) |
        (1u << 3) |
        (1u << 4) |
        (static_cast<u32>(GX_BL_INVSRCALPHA) << 5) |
        (static_cast<u32>(GX_BL_SRCALPHA) << 8) |
        (static_cast<u32>(GX_LO_XOR) << 12);
    SendBpRegister(blendValue);
    SendBpRegister(
        0x40000000u |
        1u |
        (static_cast<u32>(GX_GREATER) << 1) |
        (1u << 4));
    // GX writes API back-face culling as hardware mode 1.
    SendBpRegister(0x00000000u | (1u << 14));
    SendBpRegister(0x22000000u | 12u | (18u << 8));
    SendBpRegister(
        0xc0000000u |
        (static_cast<u32>(GX_CC_ZERO) << 12) |
        (static_cast<u32>(GX_CC_TEXC) << 8) |
        (static_cast<u32>(GX_CC_RASC) << 4) |
        static_cast<u32>(GX_CC_ZERO));
    SendBpRegister(
        0xf3000000u |
        60u |
        (40u << 8) |
        (static_cast<u32>(GX_LESS) << 16) |
        (static_cast<u32>(GX_GREATER) << 19) |
        (static_cast<u32>(GX_AOP_XOR) << 22));
    SendBpRegister(0xee000000u | 0x0003e800u);
    SendBpRegister(0xef000000u | 0x00400000u);
    SendBpRegister(0xf0000000u | 2u);
    SendBpRegister(
        0xf1000000u |
        0x0003e000u |
        (static_cast<u32>(GX_FOG_EXPONENT2) << 21));
    SendBpRegister(
        0xf2000000u |
        (0x80u << 16) |
        (0x40u << 8) |
        0x20u);
    SendBpRegister(0xe9000000u | 0x120u | (0x140u << 12));
    SendBpRegister(
        0xe8000000u |
        (342u + 160u) |
        (1u << 10));

    const auto& state = SIM::GX::GetGlobalState();
    const auto& blend = state.GetBlendState();
    const auto& depth = state.GetDepthState();
    const auto& alphaCompare = state.GetAlphaCompareState();
    const auto& fog = state.GetFogState();
    const auto& raster = state.GetRasterState();
    const auto& firstStage = state.GetTevStageState(0);
    return
        blend.mode == GX_BM_BLEND &&
        blend.sourceFactor == GX_BL_SRCALPHA &&
        blend.destinationFactor == GX_BL_INVSRCALPHA &&
        blend.logicOperation == GX_LO_XOR &&
        blend.colorUpdateEnabled &&
        blend.alphaUpdateEnabled &&
        blend.ditherEnabled &&
        depth.compareEnabled &&
        depth.function == GX_GREATER &&
        depth.updateEnabled &&
        alphaCompare.comparison0 == GX_LESS &&
        alphaCompare.reference0 == 60 &&
        alphaCompare.operation == GX_AOP_XOR &&
        alphaCompare.comparison1 == GX_GREATER &&
        alphaCompare.reference1 == 40 &&
        fog.type == GX_FOG_EXPONENT2 &&
        !fog.orthographic &&
        NearlyEqual(fog.parameterA, 0.25f) &&
        fog.parameterBMagnitude == 0x400000u &&
        fog.parameterBShift == 2 &&
        NearlyEqual(fog.parameterC, 0.125f) &&
        NearlyEqual(fog.color[0], 0x80 / 255.0f) &&
        NearlyEqual(fog.color[1], 0x40 / 255.0f) &&
        NearlyEqual(fog.color[2], 0x20 / 255.0f) &&
        fog.rangeAdjustmentEnabled &&
        fog.rangeAdjustmentCenter == 160 &&
        fog.rangeAdjustmentTable[0] == 0x120 &&
        fog.rangeAdjustmentTable[1] == 0x140 &&
        raster.cullMode == GX_CULL_BACK &&
        NearlyEqual(raster.lineWidth, 2.0f) &&
        NearlyEqual(raster.pointSize, 3.0f) &&
        firstStage.colorMode == SIM::GX::TevColorMode::Modulate;
}

bool TestZeroRasterChannelContract() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();
    // GXInit normally seeds TREF0's BP address before GXSetTevOrder updates
    // its fields. This focused test does not initialize a hardware FIFO.
    gx->tref[0] = 0x28000000u;

    GXSetTevOrder(
        GX_TEVSTAGE0,
        GX_TEXCOORD_NULL,
        GX_TEXMAP_NULL,
        GX_COLOR_ZERO);
    if (SIM::GX::GetGlobalState().GetTevStageState(0).rasterChannel != 7u) {
        return false;
    }

    GXSetTevOrder(
        GX_TEVSTAGE0,
        GX_TEXCOORD_NULL,
        GX_TEXMAP_NULL,
        GX_COLOR_NULL);
    if (SIM::GX::GetGlobalState().GetTevStageState(0).rasterChannel != 7u) {
        return false;
    }

    // Keep the GLSL interpretation of the hardware selector tied to the SDK
    // contract: RAS1_CC_Z supplies zero for every raster-color component.
    return std::string_view(SIM_GXFragmentShader).find(
               "if (channel == 7) return vec4(0.0);") !=
           std::string_view::npos;
}

bool TestBigEndianBpWriteMaskCommands() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    // These are literal GameCube display-list bytes. Seed PE_CMODE0 with all
    // framebuffer updates enabled, install a mask which excludes those bits,
    // then change only the blend fields in a separate display-list call.
    const std::array<u8, 10> seedAndMask = {
        0x61, 0x41, 0x00, 0x00, 0x1c,
        0x61, 0xfe, 0x00, 0xff, 0xe3,
    };
    const std::array<u8, 5> maskedBlend = {
        0x61, 0x41, 0x00, 0x00, 0x01,
    };
    SIM_GX_CommandProcessor_CallDisplayList(
        seedAndMask.data(), static_cast<u32>(seedAndMask.size()));
    SIM_GX_CommandProcessor_CallDisplayList(
        maskedBlend.data(), static_cast<u32>(maskedBlend.size()));

    const auto& maskedState = SIM::GX::GetGlobalState();
    const auto& maskedBlendState = maskedState.GetBlendState();
    if (maskedBlendState.mode != GX_BM_BLEND ||
        !maskedBlendState.ditherEnabled ||
        !maskedBlendState.colorUpdateEnabled ||
        !maskedBlendState.alphaUpdateEnabled) {
        return false;
    }

    // The mask is one-shot. The following unmasked write must clear all of
    // those fields instead of accidentally retaining the prior mask.
    const std::array<u8, 5> unmaskedBlend = {
        0x61, 0x41, 0x00, 0x00, 0x00,
    };
    SIM_GX_CommandProcessor_CallDisplayList(
        unmaskedBlend.data(), static_cast<u32>(unmaskedBlend.size()));
    const auto& unmaskedBlendState =
        SIM::GX::GetGlobalState().GetBlendState();
    return
        unmaskedBlendState.mode == GX_BM_NONE &&
        !unmaskedBlendState.ditherEnabled &&
        !unmaskedBlendState.colorUpdateEnabled &&
        !unmaskedBlendState.alphaUpdateEnabled;
}

bool TestBpTevCompareCommands() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    SendBpRegister(
        0xe2000000u |
        0xa0u |
        (0xffu << 12));
    SendBpRegister(
        0xe3000000u |
        0xa0u |
        (0xa0u << 12));
    SendBpRegister(
        0xc0000000u |
        (static_cast<u32>(GX_CC_TEXC) << 12) |
        (static_cast<u32>(GX_CC_ZERO) << 8) |
        (static_cast<u32>(GX_CC_ONE) << 4) |
        static_cast<u32>(GX_CC_C0) |
        (3u << 16) |
        (1u << 18) |
        (1u << 19) |
        (3u << 20));

    const auto& state = SIM::GX::GetGlobalState();
    const auto& firstStage = state.GetTevStageState(0);
    const auto& color0 = state.GetTevColor(GX_TEVREG0);
    return
        firstStage.colorMode ==
            SIM::GX::TevColorMode::CompareTextureRgb8EqualZero &&
        NearlyEqual(color0[0], 0xa0 / 255.0f) &&
        NearlyEqual(color0[1], 0xa0 / 255.0f) &&
        NearlyEqual(color0[2], 0xa0 / 255.0f) &&
        NearlyEqual(color0[3], 1.0f);
}

bool TestTevColorArgumentHardwareEncoding() {
    // Keep the shader's raw BP selector interpretation tied to Nintendo's
    // interleaved color/alpha register encoding. In particular, selector 6
    // is C2, not A1; confusing those makes valid multi-stage materials black.
    const std::string_view shader = SIM_GXFragmentShader;
    return
        shader.find("if (input == 3) return vec3(reg0.a);") !=
            std::string_view::npos &&
        shader.find("if (input == 4) return reg1.rgb;") !=
            std::string_view::npos &&
        shader.find("if (input == 5) return vec3(reg1.a);") !=
            std::string_view::npos &&
        shader.find("if (input == 6) return reg2.rgb;") !=
            std::string_view::npos;
}

bool TestTevSignedColorRegisters() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    const GXColorS10 color = {
        -1024,
        -1,
        1023,
        255,
    };
    GXSetTevColorS10(GX_TEVREG1, color);

    const auto& decoded =
        SIM::GX::GetGlobalState().GetTevColor(GX_TEVREG1);
    return
        NearlyEqual(decoded[0], -1024.0f / 255.0f) &&
        NearlyEqual(decoded[1], -1.0f / 255.0f) &&
        NearlyEqual(decoded[2], 1023.0f / 255.0f) &&
        NearlyEqual(decoded[3], 1.0f);
}

bool TestTevKonstSelections() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    const auto& initialState = SIM::GX::GetGlobalState();
    const auto defaultColor = initialState.GetTevKonstColor(GX_TEVSTAGE0);
    if (!NearlyEqual(defaultColor[0], 0.25f) ||
        !NearlyEqual(defaultColor[1], 0.25f) ||
        !NearlyEqual(defaultColor[2], 0.25f) ||
        !NearlyEqual(initialState.GetTevKonstAlpha(GX_TEVSTAGE0), 1.0f)) {
        return false;
    }

    SendBpRegister(
        0xe2000000u |
        64u |
        (32u << 12) |
        (8u << 20));
    SendBpRegister(
        0xe3000000u |
        192u |
        (128u << 12) |
        (8u << 20));
    SendBpRegister(
        0xf7000000u |
        (static_cast<u32>(GX_TEV_KCSEL_K1_G) << 14) |
        (static_cast<u32>(GX_TEV_KASEL_K1_A) << 19));

    const auto& state = SIM::GX::GetGlobalState();
    const auto selectedColor = state.GetTevKonstColor(GX_TEVSTAGE3);
    const float selectedGreen = 128.0f / 255.0f;
    return
        NearlyEqual(selectedColor[0], selectedGreen) &&
        NearlyEqual(selectedColor[1], selectedGreen) &&
        NearlyEqual(selectedColor[2], selectedGreen) &&
        NearlyEqual(selectedColor[3], selectedGreen) &&
        NearlyEqual(
            state.GetTevKonstAlpha(GX_TEVSTAGE3),
            32.0f / 255.0f);
}

bool TestBpCopyClearCommands() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    SendBpRegister(0x4f000000u | 0x20u | (0x80u << 8));
    SendBpRegister(0x50000000u | 0x60u | (0x40u << 8));
    SendBpRegister(0x51000000u | 0x007fffffu);
    SendBpRegister(0x52000000u | (1u << 11) | (1u << 14));

    auto& state = SIM::GX::GetGlobalState();
    const auto& color = state.GetCopyClearColor();
    return
        NearlyEqual(color[0], 0x20 / 255.0f) &&
        NearlyEqual(color[1], 0x40 / 255.0f) &&
        NearlyEqual(color[2], 0x60 / 255.0f) &&
        NearlyEqual(color[3], 0x80 / 255.0f) &&
        NearlyEqual(state.GetCopyClearDepth(), 0x7fffff / 16777215.0f) &&
        state.ConsumeCopyClearRequest() &&
        !state.ConsumeCopyClearRequest();
}

bool TestDisplayCopyRebasesViewportReference() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    // GX initialization briefly uses 480 lines before this NTSC-style demo
    // switches to a 448-line EFB and scales it into the display XFB.
    GXSetViewport(0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f);
    GXSetViewport(0.0f, 0.0f, 640.0f, 448.0f, 0.0f, 1.0f);
    GXSetScissor(0, 0, 640, 448);

    SendBpRegister(0x49000000u);
    SendBpRegister(
        0x4a000000u |
        (640u - 1u) |
        ((448u - 1u) << 10u));
    SendBpRegister(0x52000000u | (1u << 14));

    const auto& viewport = SIM::GX::GetGlobalState().GetViewportState();
    return
        NearlyEqual(viewport.referenceWidth, 640.0f) &&
        NearlyEqual(viewport.referenceHeight, 448.0f);
}

bool TestBpTextureAndScissorCommands() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    SendBpRegister(
        0x28000000u |
        (3u << 0) |
        (2u << 3) |
        (1u << 6));

    const u32 encodedTop = 20u + 342u;
    const u32 encodedLeft = 10u + 342u;
    const u32 encodedBottom = encodedTop + 200u - 1u;
    const u32 encodedRight = encodedLeft + 300u - 1u;
    SendBpRegister(
        0x20000000u |
        encodedTop |
        (encodedLeft << 12));
    SendBpRegister(
        0x21000000u |
        encodedBottom |
        (encodedRight << 12));

    alignas(32) std::array<u8, 32> image = {};
    alignas(32) std::array<u16, 16> palette = {};
    SIM_GX_CommandProcessor_LoadTlut(
        5,
        palette.data(),
        GX_TL_RGB565,
        static_cast<u16>(palette.size()));
    SIM_GX_CommandProcessor_LoadTexture(
        3,
        image.data(),
        8,
        8,
        GX_TF_I4,
        GX_CLAMP,
        GX_REPEAT,
        GX_NEAR,
        GX_LINEAR,
        GX_FALSE,
        0.0f,
        0.0f,
        0.0f,
        5);

    const auto& state = SIM::GX::GetGlobalState();
    const auto& stage = state.GetTevStageState(0);
    const auto& scissor = state.GetScissorState();
    const auto& texture = state.GetTextureState(3);
    const auto& tlut = state.GetTlutState(5);
    return
        stage.textureEnabled &&
        stage.textureMap == 3 &&
        stage.textureCoordinate == 2 &&
        scissor.valid &&
        scissor.left == 10 &&
        scissor.top == 20 &&
        scissor.width == 300 &&
        scissor.height == 200 &&
        texture.data == image.data() &&
        texture.width == 8 &&
        texture.height == 8 &&
        texture.format == GX_TF_I4 &&
        texture.wrapS == GX_CLAMP &&
        texture.wrapT == GX_REPEAT &&
        texture.minFilter == GX_NEAR &&
        texture.magFilter == GX_LINEAR &&
        texture.tlutName == 5 &&
        tlut.CanonicalData() != palette.data() &&
        std::memcmp(
            tlut.CanonicalData(),
            palette.data(),
            palette.size() * sizeof(palette[0])) == 0 &&
        tlut.format == GX_TL_RGB565 &&
        tlut.entries == palette.size();
}

bool TestTlutDmaSnapshotAndNativeEncoding() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    alignas(32) std::array<u8, 32> image = {};
    alignas(32) std::array<u16, 4> nativePalette = {
        0xfc00u,
        0x801fu,
        0x83e0u,
        0xffffu,
    };
    SIM_GX_CommandProcessor_LoadTexture(
        0,
        image.data(),
        8,
        8,
        GX_TF_C4,
        GX_CLAMP,
        GX_CLAMP,
        GX_NEAR,
        GX_NEAR,
        GX_FALSE,
        0.0f,
        0.0f,
        0.0f,
        3);
    const u64 initialTextureRevision =
        SIM::GX::GetGlobalState().GetTextureState(0).revision;

    GXInitTlutRegion(&TestTlutRegion, 0x80000u, GX_TLUT_256);
    const GXTlutRegionCallback previousTlutCallback =
        GXSetTlutRegionCallback(GetTestTlutRegion);
    GXTlutObj nativeTlutObject = {};
    GXInitTlutObj(
        &nativeTlutObject,
        nativePalette.data(),
        GX_TL_RGB5A3,
        static_cast<u16>(nativePalette.size()));
    GXLoadTlut(&nativeTlutObject, 3);
    GXSetTlutRegionCallback(previousTlutCallback);
    const auto& firstLoad = SIM::GX::GetGlobalState().GetTlutState(3);
    const std::array<u8, 8> expectedFirst = {
        0xfcu, 0x00u,
        0x80u, 0x1fu,
        0x83u, 0xe0u,
        0xffu, 0xffu,
    };
    if (firstLoad.CanonicalData() == nativePalette.data() ||
        firstLoad.canonicalBytes !=
            std::vector<u8>(expectedFirst.begin(), expectedFirst.end()) ||
        firstLoad.revision == 0u ||
        SIM::GX::GetGlobalState().GetTextureState(0).revision !=
            initialTextureRevision) {
        return false;
    }

    // A GXLoadTlut command snapshots source memory. Mutation after the load
    // is invisible until another load, even when the source pointer is reused.
    const u64 firstTlutRevision = firstLoad.revision;
    const u64 firstTextureRevision =
        SIM::GX::GetGlobalState().GetTextureState(0).revision;
    nativePalette[1] = 0x7c0fu;
    if (static_cast<const u8*>(firstLoad.CanonicalData())[2] != 0x80u ||
        static_cast<const u8*>(firstLoad.CanonicalData())[3] != 0x1fu) {
        return false;
    }

    SIM_GX_CommandProcessor_LoadTlutNativeU16(
        3,
        nativePalette.data(),
        GX_TL_RGB5A3,
        static_cast<u16>(nativePalette.size()));
    const auto& changedLoad = SIM::GX::GetGlobalState().GetTlutState(3);
    if (static_cast<const u8*>(changedLoad.CanonicalData())[2] != 0x7cu ||
        static_cast<const u8*>(changedLoad.CanonicalData())[3] != 0x0fu ||
        changedLoad.revision <= firstTlutRevision ||
        SIM::GX::GetGlobalState().GetTextureState(0).revision !=
            firstTextureRevision) {
        return false;
    }

    const u64 changedTlutRevision = changedLoad.revision;
    const u64 changedTextureRevision =
        SIM::GX::GetGlobalState().GetTextureState(0).revision;
    SIM_GX_CommandProcessor_LoadTlutNativeU16(
        3,
        nativePalette.data(),
        GX_TL_RGB5A3,
        static_cast<u16>(nativePalette.size()));
    if (SIM::GX::GetGlobalState().GetTlutState(3).revision !=
            changedTlutRevision ||
        SIM::GX::GetGlobalState().GetTextureState(0).revision !=
            changedTextureRevision) {
        return false;
    }

    // Standard GX TLUT input is already serialized in GameCube byte order.
    // It receives the same DMA snapshot semantics without a host byte swap.
    alignas(32) std::array<u8, 4> canonicalPalette = {
        0xfcu, 0x00u, 0x80u, 0x1fu,
    };
    SIM_GX_CommandProcessor_LoadTlut(
        4,
        canonicalPalette.data(),
        GX_TL_RGB5A3,
        2);
    const auto copiedState =
        SIM::GX::GetGlobalState().GetTlutState(4);
    const u64 canonicalRevision = copiedState.revision;
    canonicalPalette[2] = 0x7cu;
    canonicalPalette[3] = 0x0fu;
    if (static_cast<const u8*>(
            SIM::GX::GetGlobalState().GetTlutState(4).CanonicalData())[2] !=
            0x80u ||
        static_cast<const u8*>(copiedState.CanonicalData())[2] != 0x80u) {
        return false;
    }
    SIM_GX_CommandProcessor_LoadTlut(
        4,
        canonicalPalette.data(),
        GX_TL_RGB5A3,
        2);
    const auto& canonicalReload =
        SIM::GX::GetGlobalState().GetTlutState(4);
    if (canonicalReload.revision <= canonicalRevision ||
        static_cast<const u8*>(canonicalReload.CanonicalData())[2] != 0x7cu) {
        return false;
    }

    const u64 contentRevision = canonicalReload.revision;
    SIM_GX_CommandProcessor_LoadTlut(
        4,
        canonicalPalette.data(),
        GX_TL_RGB565,
        2);
    return
        SIM::GX::GetGlobalState().GetTlutState(4).revision >
            contentRevision;
}

bool TestSemanticRenderStateCache() {
    using namespace SIM::GX;
    using namespace SIM::GX::Detail;

    ViewportState viewport = {};
    ScissorState scissor = {};
    DepthState depth = {};
    RasterState raster = {};
    BlendState blend = {};
    RenderStateCache cache;

    if (cache.Update(
            viewport,
            scissor,
            depth,
            raster,
            blend,
            640,
            480) != RenderStateAll ||
        cache.Update(
            viewport,
            scissor,
            depth,
            raster,
            blend,
            640,
            480) != 0u) {
        return false;
    }

    depth.updateEnabled = false;
    if (cache.Update(
            viewport,
            scissor,
            depth,
            raster,
            blend,
            640,
            480) != RenderStateDepth) {
        return false;
    }
    blend.colorUpdateEnabled = false;
    if (cache.Update(
            viewport,
            scissor,
            depth,
            raster,
            blend,
            640,
            480) != RenderStateBlend) {
        return false;
    }
    scissor.left = 12u;
    if (cache.Update(
            viewport,
            scissor,
            depth,
            raster,
            blend,
            640,
            480) != RenderStateScissor) {
        return false;
    }
    viewport.referenceWidth = 608.0f;
    if (cache.Update(
            viewport,
            scissor,
            depth,
            raster,
            blend,
            640,
            480) !=
        (RenderStateViewport | RenderStateScissor)) {
        return false;
    }
    if (cache.Update(
            viewport,
            scissor,
            depth,
            raster,
            blend,
            1280,
            960) !=
        (RenderStateViewport | RenderStateScissor)) {
        return false;
    }

    raster.cullMode = GX_CULL_BACK;
    if (cache.Update(
            viewport,
            scissor,
            depth,
            raster,
            blend,
            1280,
            960) != RenderStateRaster) {
        return false;
    }
    cache.Invalidate();
    return cache.Update(
               viewport,
               scissor,
               depth,
               raster,
               blend,
               1280,
               960) == RenderStateAll;
}

}

int main() {
    if (!TestTextureCopyIntensityConversion()) {
        return 20;
    }
    if (!TestTextureCopyChannelSelection()) {
        return 22;
    }
    if (!TestRgb565TextureCopyEncoding()) {
        return 21;
    }
    if (!TestDepthTextureCopyEncoding()) {
        return 23;
    }
    if (!TestGXCompressZ16Vectors()) {
        return 34;
    }
    if (!TestIa8TlutTypedU16Dispatch()) {
        return 35;
    }
    if (!TestPreloadEntireMipChain()) {
        return 36;
    }
    if (!TestNativeU16TextureSourceEncoding()) {
        return 37;
    }
    if (!TestMipPayloadLayoutDecodeAndFiltering()) {
        return 38;
    }
    if (!TestNormalMatrixState()) {
        return 24;
    }
    if (!TestIndependentTextureLodSetters()) {
        return 1;
    }
    if (!TestSdkTextureLodEncoding()) {
        return 32;
    }
    if (!TestHostExiSyncCompletion()) {
        return 33;
    }
    if (!TestHostTexturePointerPreserved()) {
        return 2;
    }
    if (!TestFifoQueries()) {
        return 3;
    }
    if (!TestHostDrawSyncCompletion()) {
        return 4;
    }
    if (!TestHostPadKeyboardFallback()) {
        return 5;
    }
    if (!TestHostPadKeyboardMerge()) {
        return 26;
    }
    if (!TestZScaleOffsetCommands()) {
        return 6;
    }
    if (!TestResetWritePipeCompatibility()) {
        return 7;
    }
    if (!TestXfChannelAndLightState()) {
        return 8;
    }
    if (!TestBpRenderStateCommands()) {
        return 9;
    }
    if (!TestZeroRasterChannelContract()) {
        return 27;
    }
    if (!TestBigEndianBpWriteMaskCommands()) {
        return 25;
    }
    if (!TestBpCopyClearCommands()) {
        return 10;
    }
    if (!TestDisplayCopyRebasesViewportReference()) {
        return 11;
    }
    if (!TestBpTextureAndScissorCommands()) {
        return 12;
    }
    if (!TestTlutDmaSnapshotAndNativeEncoding()) {
        return 28;
    }
    if (!TestSemanticRenderStateCache()) {
        return 29;
    }
    if (!TestTextureObjectAbiBounds()) {
        return 13;
    }
    if (!TestPositionTextureCoordinateGeneration()) {
        return 14;
    }
    if (!TestTextureMatrix2x4ImplicitQ()) {
        return 15;
    }
    if (!TestTextureCoordinateSourceImplicitQ()) {
        return 16;
    }
    if (!TestDualTextureCoordinateGeneration()) {
        return 17;
    }
    if (!TestHostPerformanceMetrics()) {
        return 18;
    }
    if (!TestBpTevCompareCommands()) {
        return 19;
    }
    if (!TestTevColorArgumentHardwareEncoding()) {
        return 30;
    }
    if (!TestTevSignedColorRegisters()) {
        return 31;
    }
    if (!TestTevKonstSelections()) {
        return 21;
    }
    return 0;
}
