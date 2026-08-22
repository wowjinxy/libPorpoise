#ifndef LIBPORPOISE_SIM_GX_GEOMETRY_HPP
#define LIBPORPOISE_SIM_GX_GEOMETRY_HPP

#include <bit>
#include <vector>

#include <dolphin/types.h>

namespace SIM::GX {

struct RenderVertex {
    // Position
    union{
        struct {
            float x;
            float y;
            float z;
        };
        float coords[3];
    } position;

    // Normal
    union{
        struct {
            float x;
            float y;
            float z;
        };
        float coords[3];
    } normal;

    // Color0
    union {
        struct {
            float r;
            float g;
            float b;
            float a;
        };
        float values[4];
    } color0;

    union {
        struct {
            float s;
            float t;
        };
        float coords[2];
    } texCoords[8];
};

class GeometryProcessor {
 public:
  GeometryProcessor();
  ~GeometryProcessor() = default;

  void ProcessByteStream(std::vector<u8>& byteStream, std::endian endian);
 private:
  //std::vector<RenderVertex> mRenderVerts;
  RenderVertex * mRenderVerts = nullptr;
  size_t mRenderVertsSize = 0;
};

}


#endif
