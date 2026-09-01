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
  unsigned int mTevStageUniformBuffer = 0;
  unsigned int mLightsUniformBuffer = 0;

  // Shader Uniform locations
  int mProjectionLocation;
  int mModelViewLocation;
  int mTextureMtxLocation;
  int mNumTexGenLocation;
  int mTexGenLocation;
  int mTevTexMapLocation;
  int mTevStageConfigsBlock;
  int mTevStageConfigsBinding;
  int mLightConfigBlock;
  int mLightConfigBlockBinding;
  int mInitialTevColorsLocation;
  int mNumTevStagesLocation;

};

GlRenderer& GetGlRenderer();

}

#endif
