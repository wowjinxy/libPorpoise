#ifndef LIBPORPOISE_SIM_GX_GLRENDERER_HPP
#define LIBPORPOISE_SIM_GX_GLRENDERER_HPP

#include <vector>

#include <dolphin/types.h>
#include <dolphin/gx/GXEnum.h>

namespace SIM::GX {

struct RenderVertex;

class GlRenderer {
 public:
  void Draw(const RenderVertex * vertices, size_t numVertices, GXPrimitive primitive);

 private:
  void Initialize();

  unsigned int mVertexArray = 0;
  unsigned int mVertexBuffer = 0;
};

GlRenderer& GetGlRenderer();

}

#endif
