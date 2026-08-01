#include <simulator/sim_gx_Geometry.hpp>
#include <simulator/sim_gx_GlRenderer.hpp>
#include <simulator/sim_gx_State.hpp>

#include <cmath>
#include <vector>

namespace {

constexpr float kTolerance = 0.0001f;

bool NearlyEqual(float left, float right) {
    return std::fabs(left - right) <= kTolerance;
}

bool ColorsEqual(
    const SIM::GX::RenderColor& left,
    const SIM::GX::RenderColor& right) {
    return
        NearlyEqual(left.r, right.r) &&
        NearlyEqual(left.g, right.g) &&
        NearlyEqual(left.b, right.b) &&
        NearlyEqual(left.a, right.a);
}

void SetXfWord(
    SIM::GX::GlobalState& state,
    u32 address,
    u32 value) {
    state.SetXfData(
        address,
        reinterpret_cast<const u8*>(&value),
        1u);
}

void SetNumColorChannels(
    SIM::GX::GlobalState& state,
    size_t count) {
    state.SetBpRegister(static_cast<u32>(count) << 4u);
}

bool TestActiveChannelCountState() {
    SIM::GX::GlobalState state;
    if (state.GetNumColorChannels() != 0u) {
        return false;
    }

    SetNumColorChannels(state, 1u);
    if (state.GetNumColorChannels() != 1u) {
        return false;
    }
    SetNumColorChannels(state, 2u);
    if (state.GetNumColorChannels() != 2u) {
        return false;
    }

    // The XF NUMCOLORS register is the transform-unit counterpart of the BP
    // GENMODE field. Direct XF command streams must update the same state.
    SetXfWord(state, 0x1009u, 1u);
    if (state.GetNumColorChannels() != 1u) {
        return false;
    }
    SetXfWord(state, 0x1009u, 7u);
    return state.GetNumColorChannels() == 2u;
}

bool TestZeroChannelsLeaveDecodedColorsAlone() {
    SIM::GX::GlobalState state;
    SetXfWord(state, 0x100cu, 0x20406080u);
    SetXfWord(state, 0x100du, 0x90a0b0c0u);

    SIM::GX::RenderVertex vertex;
    vertex.color0 = {0.1f, 0.2f, 0.3f, 0.4f};
    vertex.color1 = {0.5f, 0.6f, 0.7f, 0.8f};
    const auto originalColor0 = vertex.color0;
    const auto originalColor1 = vertex.color1;
    std::vector<SIM::GX::RenderVertex> vertices = {vertex};

    SIM::GX::ApplyColorChannels(state, vertices);
    return
        ColorsEqual(vertices[0].color0, originalColor0) &&
        ColorsEqual(vertices[0].color1, originalColor1);
}

bool TestOneUnlitChannelMaterialSources() {
    SIM::GX::GlobalState state;
    SetNumColorChannels(state, 1u);
    SetXfWord(state, 0x100cu, 0x4080bfc0u);

    // Color comes from the vertex (bit 0); alpha comes from its material
    // register. Channel 1 is inactive and must remain untouched.
    SetXfWord(state, 0x100eu, 1u);
    SetXfWord(state, 0x1010u, 0u);
    SIM::GX::RenderVertex vertex;
    vertex.color0 = {0.1f, 0.2f, 0.3f, 0.4f};
    vertex.color1 = {0.9f, 0.8f, 0.7f, 0.6f};
    const auto inactiveColor = vertex.color1;
    std::vector<SIM::GX::RenderVertex> vertices = {vertex};
    SIM::GX::ApplyColorChannels(state, vertices);
    if (!NearlyEqual(vertices[0].color0.r, 0.1f) ||
        !NearlyEqual(vertices[0].color0.g, 0.2f) ||
        !NearlyEqual(vertices[0].color0.b, 0.3f) ||
        !NearlyEqual(vertices[0].color0.a, 192.0f / 255.0f) ||
        !ColorsEqual(vertices[0].color1, inactiveColor)) {
        return false;
    }

    // Swap the sources: RGB now comes from the material register while alpha
    // remains the decoded vertex alpha.
    SetXfWord(state, 0x100eu, 0u);
    SetXfWord(state, 0x1010u, 1u);
    vertices = {vertex};
    SIM::GX::ApplyColorChannels(state, vertices);
    return
        NearlyEqual(vertices[0].color0.r, 64.0f / 255.0f) &&
        NearlyEqual(vertices[0].color0.g, 128.0f / 255.0f) &&
        NearlyEqual(vertices[0].color0.b, 191.0f / 255.0f) &&
        NearlyEqual(vertices[0].color0.a, 0.4f);
}

bool TestTwoChannelsProcessBothColors() {
    SIM::GX::GlobalState state;
    SetNumColorChannels(state, 2u);
    SetXfWord(state, 0x100cu, 0x10203040u);
    SetXfWord(state, 0x100du, 0x50607080u);
    SetXfWord(state, 0x100eu, 0u);
    SetXfWord(state, 0x1010u, 0u);
    SetXfWord(state, 0x100fu, 0u);
    SetXfWord(state, 0x1011u, 0u);

    std::vector<SIM::GX::RenderVertex> vertices(1);
    SIM::GX::ApplyColorChannels(state, vertices);
    return
        NearlyEqual(vertices[0].color0.r, 16.0f / 255.0f) &&
        NearlyEqual(vertices[0].color0.a, 64.0f / 255.0f) &&
        NearlyEqual(vertices[0].color1.r, 80.0f / 255.0f) &&
        NearlyEqual(vertices[0].color1.g, 96.0f / 255.0f) &&
        NearlyEqual(vertices[0].color1.b, 112.0f / 255.0f) &&
        NearlyEqual(vertices[0].color1.a, 128.0f / 255.0f);
}

bool TestLightingStillUsesAmbientAndMaterial() {
    SIM::GX::GlobalState state;
    SetNumColorChannels(state, 1u);
    SetXfWord(state, 0x100au, 0x8040ffffu);
    SetXfWord(state, 0x100cu, 0x808080c0u);

    // Lighting enabled with register material/ambient and no selected lights:
    // the GX equation is material * ambient. Alpha remains unlit.
    SetXfWord(state, 0x100eu, 1u << 1u);
    SetXfWord(state, 0x1010u, 0u);
    std::vector<SIM::GX::RenderVertex> vertices(1);
    vertices[0].position = {3.0f, 4.0f, 5.0f};
    vertices[0].normal = {0.0f, 0.0f, 1.0f};
    SIM::GX::ApplyColorChannels(state, vertices);

    const float material = 128.0f / 255.0f;
    return
        NearlyEqual(
            vertices[0].color0.r,
            material * (128.0f / 255.0f)) &&
        NearlyEqual(
            vertices[0].color0.g,
            material * (64.0f / 255.0f)) &&
        NearlyEqual(vertices[0].color0.b, material) &&
        NearlyEqual(vertices[0].color0.a, 192.0f / 255.0f);
}

}

int main() {
    if (!TestActiveChannelCountState()) {
        return 1;
    }
    if (!TestZeroChannelsLeaveDecodedColorsAlone()) {
        return 2;
    }
    if (!TestOneUnlitChannelMaterialSources()) {
        return 3;
    }
    if (!TestTwoChannelsProcessBothColors()) {
        return 4;
    }
    if (!TestLightingStillUsesAmbientAndMaterial()) {
        return 5;
    }
    return 0;
}
