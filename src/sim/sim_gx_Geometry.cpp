#include "simulator/sim_gx_Geometry.hpp"

#include "simulator/sim_gx_GlRenderer.hpp"
#include "simulator/sim_gx_State.hpp"

namespace SIM::GX {

GeometryProcessor::GeometryProcessor() = default;

void GeometryProcessor::ProcessByteStream(
    const std::vector<u8>& byteStream,
    bool bigEndian) {
    auto& state = GetGlobalState();
    if (!DecodeVertexStream(state, byteStream, bigEndian, mRenderVerts)) {
        return;
    }
    // DecodeVertexStream clears and completely rebuilds this owned buffer on
    // every primitive, so renderer-side host transforms can safely run in
    // place without preserving the decoded values for a later draw.
    GetGlRenderer().Draw(mRenderVerts, state.GetCurrentPrimitive());
}

}
