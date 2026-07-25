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
    return 0;
}
