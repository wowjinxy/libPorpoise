#ifndef LIBPORPOISE_SIM_GX_GEOMETRY_HPP
#define LIBPORPOISE_SIM_GX_GEOMETRY_HPP

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
};

class GeometryProcessor {
 public:
  GeometryProcessor();
  ~GeometryProcessor() = default;

  void ProcessByteStream(std::vector<u8>& byteStream);
 private:
  std::vector<RenderVertex> mRenderVerts;
};

}


#endif
