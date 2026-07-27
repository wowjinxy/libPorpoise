#include "simulator/sim_gx_Geometry.hpp"
#include "simulator/sim_gx_State.hpp"

#include <array>
#include <cmath>
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
    if (!TestMalformedStreamClearsOutput()) {
        return 4;
    }
    if (!TestViewportTransformState()) {
        return 5;
    }
    if (!TestIndexedPositionMatrixState()) {
        return 6;
    }
    if (!TestMultiStageTevState()) {
        return 7;
    }
    if (!TestTexGenTypeDecode()) {
        return 8;
    }
    return 0;
}
