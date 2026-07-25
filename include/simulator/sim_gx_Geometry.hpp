#ifndef LIBPORPOISE_SIM_GX_GEOMETRY_HPP
#define LIBPORPOISE_SIM_GX_GEOMETRY_HPP

#include <array>
#include <span>
#include <vector>

#include <dolphin/types.h>

namespace SIM::GX {

class GlobalState;

struct RenderVector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    float* Data() { return &x; }
    const float* Data() const { return &x; }
};

struct RenderColor {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;

    float* Data() { return &r; }
    const float* Data() const { return &r; }
};

struct RenderTexCoord {
    float s = 0.0f;
    float t = 0.0f;

    float* Data() { return &s; }
    const float* Data() const { return &s; }
};

struct RenderVertex {
    RenderVector3 position;
    RenderVector3 normal;
    RenderVector3 binormal;
    RenderVector3 tangent;
    RenderColor color0;
    RenderColor color1;
    std::array<RenderTexCoord, 8> texCoords = {};
    u8 positionMatrixIndex = 0;
    std::array<u8, 8> textureMatrixIndices = {};
};

bool DecodeVertexStream(
    const GlobalState& state,
    std::span<const u8> byteStream,
    bool bigEndian,
    std::vector<RenderVertex>& output);

class GeometryProcessor {
 public:
  GeometryProcessor();
  ~GeometryProcessor() = default;

  void ProcessByteStream(const std::vector<u8>& byteStream, bool bigEndian);
 private:
  std::vector<RenderVertex> mRenderVerts;
};

}


#endif
