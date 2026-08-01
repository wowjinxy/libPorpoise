#include "simulator/sim_gx_Geometry.hpp"
#include "simulator/sim_gx_GlRenderer.hpp"
#include "simulator/sim_gx_State.hpp"

#include <array>
#include <cmath>
#include <string_view>
#include <vector>

namespace {

constexpr float kTolerance = 0.0001f;

bool NearlyEqual(float left, float right) {
    return std::fabs(left - right) <= kTolerance;
}

bool TestDirectAttributes() {
    SIM::GX::GlobalState state;
    state.SetVertexDescriptor(GX_VA_PNMTXIDX, GX_DIRECT);
    state.SetVertexDescriptor(GX_VA_TEX0MTXIDX, GX_DIRECT);
    state.SetVertexDescriptor(GX_VA_POS, GX_DIRECT);
    state.SetVertexDescriptor(GX_VA_NRM, GX_DIRECT);
    state.SetVertexDescriptor(GX_VA_CLR0, GX_DIRECT);
    state.SetVertexDescriptor(GX_VA_CLR1, GX_DIRECT);
    state.SetVertexDescriptor(GX_VA_TEX0, GX_DIRECT);
    state.SetVertexDescriptor(GX_VA_TEX1, GX_DIRECT);

    state.SetVertexFormatComponents(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ);
    state.SetVertexFormatDataType(GX_VTXFMT0, GX_VA_POS, GX_S16);
    state.SetVertexFormatFraction(GX_VTXFMT0, GX_VA_POS, 4);
    state.SetVertexFormatComponents(GX_VTXFMT0, GX_VA_NRM, GX_NRM_NBT);
    state.SetVertexFormatDataType(GX_VTXFMT0, GX_VA_NRM, GX_S8);
    state.SetVertexFormatComponents(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA);
    state.SetVertexFormatDataType(GX_VTXFMT0, GX_VA_CLR0, GX_RGBA8);
    state.SetVertexFormatComponents(GX_VTXFMT0, GX_VA_CLR1, GX_CLR_RGB);
    state.SetVertexFormatDataType(GX_VTXFMT0, GX_VA_CLR1, GX_RGB565);
    state.SetVertexFormatComponents(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST);
    state.SetVertexFormatDataType(GX_VTXFMT0, GX_VA_TEX0, GX_S16);
    state.SetVertexFormatFraction(GX_VTXFMT0, GX_VA_TEX0, 1);
    state.SetVertexFormatComponents(GX_VTXFMT0, GX_VA_TEX1, GX_TEX_S);
    state.SetVertexFormatDataType(GX_VTXFMT0, GX_VA_TEX1, GX_U8);
    state.SetVertexFormatFraction(GX_VTXFMT0, GX_VA_TEX1, 2);

    const std::vector<u8> stream = {
        0x45, 0x7f,
        0x00, 0x18, 0xff, 0xe0, 0x00, 0x08,
        0x40, 0x00, 0x00,
        0x00, 0x40, 0x00,
        0x00, 0x00, 0xc0,
        10, 20, 30, 40,
        0xf8, 0x00,
        0x00, 0x03, 0xff, 0xfc,
        6,
    };

    std::vector<SIM::GX::RenderVertex> decoded;
    if (!SIM::GX::DecodeVertexStream(state, stream, true, decoded) ||
        decoded.size() != 1) {
        return false;
    }

    const auto& vertex = decoded.front();
    return
        vertex.positionMatrixIndex == 5 &&
        vertex.textureMatrixIndices[0] == 63 &&
        NearlyEqual(vertex.position.x, 1.5f) &&
        NearlyEqual(vertex.position.y, -2.0f) &&
        NearlyEqual(vertex.position.z, 0.5f) &&
        NearlyEqual(vertex.normal.x, 1.0f) &&
        NearlyEqual(vertex.binormal.y, 1.0f) &&
        NearlyEqual(vertex.tangent.z, -1.0f) &&
        NearlyEqual(vertex.color0.r, 10.0f / 255.0f) &&
        NearlyEqual(vertex.color0.a, 40.0f / 255.0f) &&
        NearlyEqual(vertex.color1.r, 1.0f) &&
        NearlyEqual(vertex.color1.g, 0.0f) &&
        NearlyEqual(vertex.texCoords[0].s, 1.5f) &&
        NearlyEqual(vertex.texCoords[0].t, -2.0f) &&
        NearlyEqual(vertex.texCoords[1].s, 1.5f);
}

bool TestIndependentlyIndexedNbt() {
    SIM::GX::GlobalState state;
    state.SetVertexDescriptor(GX_VA_NRM, GX_INDEX8);
    state.SetVertexFormatComponents(GX_VTXFMT0, GX_VA_NRM, GX_NRM_NBT3);
    state.SetVertexFormatDataType(GX_VTXFMT0, GX_VA_NRM, GX_S16);

    std::array<s16, 18> normals = {};
    normals[0] = 16384;
    normals[6 + 2] = -16384;
    normals[9 + 3 + 1] = 16384;
    state.SetVertexArray(
        GX_VA_NBT,
        {normals.data(), static_cast<int>(9 * sizeof(s16))});

    const std::vector<u8> stream = {0, 1, 0};
    std::vector<SIM::GX::RenderVertex> decoded;
    if (!SIM::GX::DecodeVertexStream(state, stream, true, decoded) ||
        decoded.size() != 1) {
        return false;
    }

    const auto& vertex = decoded.front();
    return
        NearlyEqual(vertex.normal.x, 1.0f) &&
        NearlyEqual(vertex.binormal.y, 1.0f) &&
        NearlyEqual(vertex.tangent.z, -1.0f);
}

bool TestIndexedVertexSuppression() {
    SIM::GX::GlobalState state;
    state.SetVertexDescriptor(GX_VA_POS, GX_INDEX8);
    state.SetVertexFormatComponents(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ);
    state.SetVertexFormatDataType(GX_VTXFMT0, GX_VA_POS, GX_S16);
    state.SetVertexFormatFraction(GX_VTXFMT0, GX_VA_POS, 4);

    std::array<s16, 6> positions = {
        16, 32, 48,
        -16, -32, -48,
    };
    state.SetVertexArray(
        GX_VA_POS,
        {
            positions.data(),
            static_cast<int>(3 * sizeof(s16)),
        });

    const std::vector<u8> stream = {0, 0xff, 1};
    std::vector<SIM::GX::RenderVertex> decoded;
    if (!SIM::GX::DecodeVertexStream(state, stream, true, decoded) ||
        decoded.size() != 2) {
        return false;
    }

    return
        NearlyEqual(decoded[0].position.x, 1.0f) &&
        NearlyEqual(decoded[0].position.z, 3.0f) &&
        NearlyEqual(decoded[1].position.x, -1.0f) &&
        NearlyEqual(decoded[1].position.z, -3.0f);
}

bool TestIndexedPackedU32Color() {
    SIM::GX::GlobalState state;
    state.SetVertexDescriptor(GX_VA_CLR0, GX_INDEX8);
    state.SetVertexFormatComponents(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA);
    state.SetVertexFormatDataType(GX_VTXFMT0, GX_VA_CLR0, GX_RGBA8);

    std::array<u32, 2> colors = {
        0x00ff0000u,
        0x12345678u,
    };
    state.SetVertexArray(
        GX_VA_CLR0,
        {
            colors.data(),
            static_cast<int>(sizeof(u32)),
            true,
        });

    const std::vector<u8> stream = {0, 1};
    std::vector<SIM::GX::RenderVertex> decoded;
    if (!SIM::GX::DecodeVertexStream(state, stream, true, decoded) ||
        decoded.size() != 2) {
        return false;
    }

    return
        NearlyEqual(decoded[0].color0.r, 0.0f) &&
        NearlyEqual(decoded[0].color0.g, 1.0f) &&
        NearlyEqual(decoded[0].color0.b, 0.0f) &&
        NearlyEqual(decoded[0].color0.a, 0.0f) &&
        NearlyEqual(decoded[1].color0.r, 0x12 / 255.0f) &&
        NearlyEqual(decoded[1].color0.g, 0x34 / 255.0f) &&
        NearlyEqual(decoded[1].color0.b, 0x56 / 255.0f) &&
        NearlyEqual(decoded[1].color0.a, 0x78 / 255.0f);
}

bool TestMalformedStreamClearsOutput() {
    SIM::GX::GlobalState state;
    state.SetVertexDescriptor(GX_VA_POS, GX_DIRECT);
    state.SetVertexFormatComponents(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ);
    state.SetVertexFormatDataType(GX_VTXFMT0, GX_VA_POS, GX_S16);

    const std::vector<u8> stream = {0, 1, 0, 2, 0};
    std::vector<SIM::GX::RenderVertex> decoded(1);
    return !SIM::GX::DecodeVertexStream(state, stream, true, decoded) &&
           decoded.empty();
}

bool TestViewportTransformState() {
    SIM::GX::GlobalState state;
    const std::array<float, 6> viewport = {
        320.0f, -240.0f, 8388608.0f, 662.0f, 582.0f, 12582912.0f,
    };
    state.SetXfData(
        0x101A,
        reinterpret_cast<const u8*>(viewport.data()),
        viewport.size());

    if (!state.HasViewportTransform()) {
        return false;
    }
    const auto& decoded = state.GetViewportTransform();
    for (size_t index = 0; index < viewport.size(); ++index) {
        if (!NearlyEqual(decoded[index], viewport[index])) {
            return false;
        }
    }
    return true;
}

bool TestIndexedPositionMatrixState() {
    SIM::GX::GlobalState state;
    const std::array<float, 12> matrix = {
        1.0f, 0.0f, 0.0f, 10.0f,
        0.0f, 1.0f, 0.0f, 20.0f,
        0.0f, 0.0f, 1.0f, 30.0f,
    };
    state.SetXfData(
        12,
        reinterpret_cast<const u8*>(matrix.data()),
        matrix.size());

    const auto& decoded = state.GetPositionMatrix(1);
    return
        NearlyEqual(decoded[3], 10.0f) &&
        NearlyEqual(decoded[7], 20.0f) &&
        NearlyEqual(decoded[11], 30.0f) &&
        NearlyEqual(state.GetPositionMatrix(0)[0], 1.0f);
}

bool TestMultiStageTevState() {
    SIM::GX::GlobalState state;

    const u32 textureReference =
        (0x28u << 24u) |
        3u |
        (4u << 3u) |
        (1u << 6u) |
        (1u << 7u);
    state.SetBpRegister(textureReference);

    const u32 colorEnvironment =
        (0xc0u << 24u) |
        GX_CC_RASC |
        (GX_CC_TEXC << 4u) |
        (GX_CC_CPREV << 8u) |
        (GX_CC_ZERO << 12u) |
        (GX_TB_ADDHALF << 16u) |
        (1u << 19u) |
        (GX_CS_SCALE_2 << 20u) |
        (GX_TEVREG2 << 22u);
    state.SetBpRegister(colorEnvironment);

    const u32 alphaEnvironment =
        (0xc1u << 24u) |
        (GX_CA_RASA << 4u) |
        (GX_CA_TEXA << 7u) |
        (GX_CA_APREV << 10u) |
        (GX_CA_ZERO << 13u) |
        (1u << 18u) |
        (1u << 19u) |
        (GX_CS_DIVIDE_2 << 20u) |
        (GX_TEVREG1 << 22u);
    state.SetBpRegister(alphaEnvironment);

    const auto& stage = state.GetTevStageState(0);
    if (stage.textureMap != 3 ||
        stage.textureCoordinate != 4 ||
        !stage.textureEnabled ||
        stage.rasterChannel != 1 ||
        stage.colorInputs[1] != GX_CC_CPREV ||
        stage.colorInputs[2] != GX_CC_TEXC ||
        stage.colorInputs[3] != GX_CC_RASC ||
        stage.colorBias != GX_TB_ADDHALF ||
        stage.colorScale != GX_CS_SCALE_2 ||
        !stage.colorClamp ||
        stage.colorOutput != GX_TEVREG2 ||
        stage.alphaInputs[1] != GX_CA_APREV ||
        stage.alphaInputs[2] != GX_CA_TEXA ||
        stage.alphaInputs[3] != GX_CA_RASA ||
        stage.alphaOperation != GX_TEV_SUB ||
        stage.alphaScale != GX_CS_DIVIDE_2 ||
        !stage.alphaClamp ||
        stage.alphaOutput != GX_TEVREG1) {
        return false;
    }

    const u32 embossTexGen =
        (1u << 4u) |
        (2u << 12u) |
        (3u << 15u);
    state.SetXfData(
        0x1040,
        reinterpret_cast<const u8*>(&embossTexGen),
        1);
    const auto& texGen = state.GetTexCoordGenState(0);
    return
        texGen.function == GX_TG_BUMP3 &&
        texGen.source == GX_TG_TEXCOORD2 &&
        texGen.embossSource == 2 &&
        texGen.embossLight == 3;
}

bool TestTexGenTypeDecode() {
    SIM::GX::GlobalState state;

    const u32 matrix2x4 = 1u << 1u;
    state.SetXfData(
        0x1040,
        reinterpret_cast<const u8*>(&matrix2x4),
        1);
    if (state.GetTexCoordGenState(0).function != GX_TG_MTX2x4) {
        return false;
    }

    const u32 matrix3x4 = 0;
    state.SetXfData(
        0x1040,
        reinterpret_cast<const u8*>(&matrix3x4),
        1);
    return state.GetTexCoordGenState(0).function == GX_TG_MTX3x4;
}

bool TestActiveTexGenCount() {
    SIM::GX::GlobalState state;
    if (state.GetNumTexGens() != 1u) {
        return false;
    }

    SIM::GX::RenderVertex vertex;
    vertex.texCoords[0] = {0.25f, 0.5f, 0.25f};
    vertex.texCoords[1] = {0.75f, 0.875f, 0.5f};
    std::vector<SIM::GX::RenderVertex> vertices = {vertex};
    SIM::GX::ApplyTextureCoordinateGeneration(state, vertices);
    if (!NearlyEqual(vertices[0].texCoords[0].s, 0.25f) ||
        !NearlyEqual(vertices[0].texCoords[0].t, 0.5f) ||
        !NearlyEqual(vertices[0].texCoords[0].q, 1.0f) ||
        !NearlyEqual(vertices[0].texCoords[1].s, 0.75f) ||
        !NearlyEqual(vertices[0].texCoords[1].t, 0.875f) ||
        !NearlyEqual(vertices[0].texCoords[1].q, 0.5f)) {
        return false;
    }

    // GENMODE bits 0..3 are the active texgen count. Once slot 1 becomes
    // active its default generator consumes TEX0, while inactive slots above
    // it remain untouched.
    state.SetBpRegister(2u);
    if (state.GetNumTexGens() != 2u) {
        return false;
    }
    vertices = {vertex};
    SIM::GX::ApplyTextureCoordinateGeneration(state, vertices);
    if (!NearlyEqual(vertices[0].texCoords[1].s, 0.25f) ||
        !NearlyEqual(vertices[0].texCoords[1].t, 0.5f) ||
        !NearlyEqual(vertices[0].texCoords[1].q, 1.0f)) {
        return false;
    }

    state.SetBpRegister(0u);
    vertices = {vertex};
    SIM::GX::ApplyTextureCoordinateGeneration(state, vertices);
    if (state.GetNumTexGens() != 0u ||
        !NearlyEqual(vertices[0].texCoords[0].q, 0.25f) ||
        !NearlyEqual(vertices[0].texCoords[1].q, 0.5f)) {
        return false;
    }

    state.SetBpRegister(0x0fu);
    return state.GetNumTexGens() == 8u;
}

bool TestTexGenOriginalAndGeneratedSourcesRemainDistinct() {
    SIM::GX::GlobalState state;
    state.SetBpRegister(2u);

    // Generator zero overwrites TEXCOORD0 from position. Generator one reads
    // GX_TG_TEX0, which is the original vertex attribute rather than the
    // result just written by generator zero.
    const std::array<u32, 2> generators = {
        0u,
        5u << 7u,
    };
    state.SetXfData(
        0x1040u,
        reinterpret_cast<const u8*>(generators.data()),
        generators.size());

    SIM::GX::RenderVertex vertex;
    vertex.position = {9.0f, 8.0f, 7.0f};
    vertex.texCoords[0] = {0.25f, 0.5f, 0.75f};
    vertex.texCoords[2] = {0.125f, 0.375f, 0.625f};
    std::vector<SIM::GX::RenderVertex> vertices = {vertex};
    SIM::GX::ApplyTextureCoordinateGeneration(state, vertices);

    return
        NearlyEqual(vertices[0].texCoords[0].s, 9.0f) &&
        NearlyEqual(vertices[0].texCoords[0].t, 8.0f) &&
        NearlyEqual(vertices[0].texCoords[0].q, 7.0f) &&
        NearlyEqual(vertices[0].texCoords[1].s, 0.25f) &&
        NearlyEqual(vertices[0].texCoords[1].t, 0.5f) &&
        NearlyEqual(vertices[0].texCoords[1].q, 0.75f) &&
        NearlyEqual(vertices[0].texCoords[2].s, 0.125f) &&
        NearlyEqual(vertices[0].texCoords[2].t, 0.375f) &&
        NearlyEqual(vertices[0].texCoords[2].q, 0.625f);
}

bool TestTevSwapState() {
    SIM::GX::GlobalState state;

    const u32 alphaEnvironment =
        (0xc1u << 24u) |
        GX_TEV_SWAP2 |
        (GX_TEV_SWAP3 << 2u);
    state.SetBpRegister(alphaEnvironment);

    const u32 tableRedGreen =
        (0xf8u << 24u) |
        GX_CH_ALPHA |
        (GX_CH_BLUE << 2u);
    const u32 tableBlueAlpha =
        (0xf9u << 24u) |
        GX_CH_GREEN |
        (GX_CH_RED << 2u);
    state.SetBpRegister(tableRedGreen);
    state.SetBpRegister(tableBlueAlpha);

    const auto& stage = state.GetTevStageState(0);
    const auto& table = state.GetTevSwapTable(1);
    return
        stage.rasterSwapTable == GX_TEV_SWAP2 &&
        stage.textureSwapTable == GX_TEV_SWAP3 &&
        table[0] == GX_CH_ALPHA &&
        table[1] == GX_CH_BLUE &&
        table[2] == GX_CH_GREEN &&
        table[3] == GX_CH_RED;
}

bool TestZTextureState() {
    SIM::GX::GlobalState state;

    state.SetBpRegister((0xf4u << 24u) | 0x00ff8000u);
    state.SetBpRegister(
        (0xf5u << 24u) |
        1u |
        (static_cast<u32>(GX_ZT_REPLACE) << 2u));

    const auto& zTexture = state.GetZTextureState();
    return
        zTexture.operation == GX_ZT_REPLACE &&
        zTexture.format == GX_TF_Z16 &&
        zTexture.bias == 0x00ff8000u;
}

bool TestTextureInvalidationState() {
    SIM::GX::GlobalState state;
    std::array<u8, 32> textureData = {};

    SIM::GX::TextureState texture;
    texture.data = textureData.data();
    texture.width = 8;
    texture.height = 8;
    texture.format = GX_TF_I4;
    state.LoadTexture(0, texture);
    state.LoadTexture(3, texture);

    const u64 firstRevision = state.GetTextureState(0).revision;
    const u64 secondRevision = state.GetTextureState(3).revision;
    const u64 firstInvalidation =
        state.GetTextureInvalidationRevision();
    if (firstRevision == 0 || secondRevision == 0 ||
        state.GetTextureState(1).revision != 0) {
        return false;
    }

    // Repeating an unchanged GXLoadTexObj descriptor is a cache hit, not a
    // texture-content change.
    state.LoadTexture(0, texture);
    if (state.GetTextureState(0).revision != firstRevision) {
        return false;
    }

    // GXInvalidateTexAll emits these BP commands. Texture source memory may
    // have changed in place, so record a lazy global validation request while
    // preserving descriptor revisions and untouched texture slots.
    state.SetBpRegister(0x66001000u);
    const u64 secondInvalidation =
        state.GetTextureInvalidationRevision();
    if (secondInvalidation <= firstInvalidation ||
        state.GetTextureState(0).revision != firstRevision ||
        state.GetTextureState(3).revision != secondRevision ||
        state.GetTextureState(0).data != textureData.data() ||
        state.GetTextureState(1).revision != 0) {
        return false;
    }

    // The companion invalidate command and region-invalidate encodings must
    // remain observable commands even though their BP payload differs.
    state.SetBpRegister(0x66001100u);
    return
        state.GetTextureInvalidationRevision() > secondInvalidation &&
        state.GetTextureState(0).revision == firstRevision &&
        state.GetTextureState(3).revision == secondRevision;
}

bool TestTextureContentSnapshot() {
    // Encoded GX textures include complete tiled blocks, including padding at
    // non-block-aligned dimensions.
    std::array<u8, 128> textureData = {};
    std::array<u8, 32> tlutData = {};

    SIM::GX::TextureState texture;
    texture.data = textureData.data();
    texture.width = 9;
    texture.height = 9;
    texture.format = static_cast<GXTexFmt>(GX_TF_C4);
    if (SIM::GX::GetTextureSourceByteSize(texture) != 128u) {
        return false;
    }

    // GX texture dimensions use 10-bit size-minus-one fields. Invalid host
    // descriptors must be rejected before any source read or RGBA allocation.
    SIM::GX::TextureState oversizedTexture = texture;
    oversizedTexture.width = 1025u;
    if (SIM::GX::GetTextureSourceByteSize(oversizedTexture) != 0u) {
        return false;
    }
    SIM::GX::TextureContentSnapshot oversizedSnapshot;
    oversizedSnapshot.Capture(oversizedTexture, nullptr);
    if (oversizedSnapshot.Matches(oversizedTexture, nullptr)) {
        return false;
    }

    SIM::GX::TlutState tlut;
    tlut.data = tlutData.data();
    tlut.format = GX_TL_RGB5A3;
    tlut.entries = 16;

    SIM::GX::TextureContentSnapshot snapshot;
    if (snapshot.Matches(texture, &tlut)) {
        return false;
    }
    snapshot.Capture(texture, &tlut);
    if (!snapshot.Matches(texture, &tlut)) {
        return false;
    }

    // Changes anywhere in the padded source tile or palette are observable.
    textureData.back() = 1;
    if (snapshot.Matches(texture, &tlut)) {
        return false;
    }
    snapshot.Capture(texture, &tlut);
    tlutData.back() = 1;
    if (snapshot.Matches(texture, &tlut)) {
        return false;
    }

    snapshot.Capture(texture, &tlut);
    texture.wrapS = GX_REPEAT;
    return snapshot.Matches(texture, &tlut);
}

bool TestTlutRevisionInvalidatesIndexedTexture() {
    using SIM::GX::Detail::ShouldValidateTexture;

    if (ShouldValidateTexture(
            false, true, 7u, 7u, 11u, 11u, 13u, 13u)) {
        return false;
    }
    if (!ShouldValidateTexture(
            false, true, 7u, 7u, 11u, 11u, 13u, 14u)) {
        return false;
    }

    // A TLUT update cannot affect a direct-color texture, while descriptor,
    // explicit invalidation, and first-object changes remain observable.
    return
        !ShouldValidateTexture(
            false, false, 7u, 7u, 11u, 11u, 13u, 14u) &&
        ShouldValidateTexture(
            false, false, 7u, 8u, 11u, 11u, 13u, 13u) &&
        ShouldValidateTexture(
            false, false, 7u, 7u, 11u, 12u, 13u, 13u) &&
        ShouldValidateTexture(
            true, false, 7u, 7u, 11u, 11u, 13u, 13u);
}

bool TestCanonicalBigEndianTlutDecode() {
    // RGB5A3 opaque red is serialized as FC 00 in GameCube memory. Decode
    // wire bytes explicitly instead of treating them as a host-native u16.
    const std::array<u8, 2> opaqueRed = {0xfcu, 0x00u};
    const SIM::GX::DecodedTlutColor decoded =
        SIM::GX::DecodeTlutEntry(GX_TL_RGB5A3, opaqueRed.data());
    if (decoded.red != 255u || decoded.green != 0u ||
        decoded.blue != 0u || decoded.alpha != 255u) {
        return false;
    }

    // Preserve a discriminator for the exact double-swap failure mode.
    const std::array<u8, 2> swapped = {0x00u, 0xfcu};
    const SIM::GX::DecodedTlutColor swappedDecoded =
        SIM::GX::DecodeTlutEntry(GX_TL_RGB5A3, swapped.data());
    return swappedDecoded.red != decoded.red ||
           swappedDecoded.green != decoded.green ||
           swappedDecoded.blue != decoded.blue ||
           swappedDecoded.alpha != decoded.alpha;
}

bool TestShaderUniformLocationCache() {
    SIM::GX::Detail::ShaderUniformLocationCache cache;
    constexpr size_t locationCount =
        SIM::GX::Detail::ShaderUniformLocationCache::LocationCount();
    static_assert(locationCount == 35u);

    size_t resolverCalls = 0;
    std::string_view firstName;
    std::string_view lastName;
    const auto resolver = [&](unsigned int program, const char* name) {
        if (resolverCalls % locationCount == 0u) {
            firstName = name;
        }
        lastName = name;
        ++resolverCalls;
        return static_cast<int>(program * 100u + resolverCalls);
    };

    const auto& first = cache.Resolve(7u, resolver);
    const int firstProjection =
        first[SIM::GX::Detail::ShaderUniform::Projection];
    if (resolverCalls != locationCount ||
        firstName != "u_projection" ||
        lastName != "u_ztexture_bias" ||
        firstProjection != 701) {
        return false;
    }

    const auto& unchanged = cache.Resolve(7u, resolver);
    if (resolverCalls != locationCount ||
        unchanged[SIM::GX::Detail::ShaderUniform::Projection] !=
            firstProjection) {
        return false;
    }

    const auto& changedProgram = cache.Resolve(8u, resolver);
    if (resolverCalls != locationCount * 2u ||
        changedProgram[SIM::GX::Detail::ShaderUniform::Projection] != 836) {
        return false;
    }

    cache.Invalidate();
    cache.Resolve(8u, resolver);
    return resolverCalls == locationCount * 3u;
}

bool TestDecodeRebuildsReusableOutput() {
    SIM::GX::GlobalState state;
    state.SetVertexDescriptor(GX_VA_POS, GX_DIRECT);
    state.SetVertexFormatComponents(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ);
    state.SetVertexFormatDataType(GX_VTXFMT0, GX_VA_POS, GX_U8);
    state.SetVertexFormatFraction(GX_VTXFMT0, GX_VA_POS, 0);

    std::vector<SIM::GX::RenderVertex> output(4);
    const std::array<u8, 6> firstStream = {1, 2, 3, 4, 5, 6};
    if (!SIM::GX::DecodeVertexStream(
            state, firstStream, true, output) ||
        output.size() != 2u ||
        !NearlyEqual(output[1].position.x, 4.0f)) {
        return false;
    }

    // Model the renderer mutating the decoded buffer, then verify the next
    // primitive replaces both its contents and its previous vertex count.
    output[0].position.x = 999.0f;
    const std::array<u8, 3> secondStream = {7, 8, 9};
    return
        SIM::GX::DecodeVertexStream(state, secondStream, true, output) &&
        output.size() == 1u &&
        NearlyEqual(output[0].position.x, 7.0f) &&
        NearlyEqual(output[0].position.y, 8.0f) &&
        NearlyEqual(output[0].position.z, 9.0f);
}

}

int main() {
    if (!TestDirectAttributes()) {
        return 1;
    }
    if (!TestIndependentlyIndexedNbt()) {
        return 2;
    }
    if (!TestIndexedVertexSuppression()) {
        return 3;
    }
    if (!TestIndexedPackedU32Color()) {
        return 4;
    }
    if (!TestMalformedStreamClearsOutput()) {
        return 5;
    }
    if (!TestViewportTransformState()) {
        return 6;
    }
    if (!TestIndexedPositionMatrixState()) {
        return 7;
    }
    if (!TestMultiStageTevState()) {
        return 8;
    }
    if (!TestTexGenTypeDecode()) {
        return 9;
    }
    if (!TestTevSwapState()) {
        return 10;
    }
    if (!TestZTextureState()) {
        return 11;
    }
    if (!TestTextureInvalidationState()) {
        return 12;
    }
    if (!TestTextureContentSnapshot()) {
        return 13;
    }
    if (!TestActiveTexGenCount()) {
        return 14;
    }
    if (!TestShaderUniformLocationCache()) {
        return 15;
    }
    if (!TestDecodeRebuildsReusableOutput()) {
        return 16;
    }
    if (!TestCanonicalBigEndianTlutDecode()) {
        return 17;
    }
    if (!TestTlutRevisionInvalidatesIndexedTexture()) {
        return 18;
    }
    if (!TestTexGenOriginalAndGeneratedSourcesRemainDistinct()) {
        return 19;
    }
    return 0;
}
