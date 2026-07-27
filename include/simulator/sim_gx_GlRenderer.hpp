#ifndef LIBPORPOISE_SIM_GX_GLRENDERER_HPP
#define LIBPORPOISE_SIM_GX_GLRENDERER_HPP

#include <array>
#include <vector>

#include <dolphin/types.h>
#include <dolphin/gx/GXEnum.h>

namespace SIM::GX {

class GlobalState;
struct RenderVertex;

void ApplyTextureCoordinateGeneration(
    const GlobalState& state,
    std::vector<RenderVertex>& vertices);

u8 ConvertRgbToCopyIntensity(u8 red, u8 green, u8 blue);

class GlRenderer {
 public:
  void Draw(const std::vector<RenderVertex>& vertices, GXPrimitive primitive);

 private:
  void Initialize();

  unsigned int mVertexArray = 0;
  unsigned int mVertexBuffer = 0;
  std::array<unsigned int, 8> mTextures = {};
  std::array<u64, 8> mTextureRevisions = {};
  int mDrawableWidth = 640;
  int mDrawableHeight = 480;
};

GlRenderer& GetGlRenderer();

}

#endif
