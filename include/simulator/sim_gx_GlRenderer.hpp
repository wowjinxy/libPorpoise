#ifndef LIBPORPOISE_SIM_GX_GLRENDERER_HPP
#define LIBPORPOISE_SIM_GX_GLRENDERER_HPP

#include <array>
#include <cstddef>
#include <vector>

#include <dolphin/types.h>
#include <dolphin/gx/GXEnum.h>
#include <simulator/sim_gx_State.hpp>

namespace SIM::GX {

struct RenderVertex;

struct DecodedTlutColor {
  u8 red = 0;
  u8 green = 0;
  u8 blue = 0;
  u8 alpha = 0;
};

// GX texture invalidation makes in-place source-memory changes visible, but
// does not imply that the source bytes actually changed. This exact snapshot
// lets the host renderer validate invalidated texture memory without decoding
// and uploading every unchanged texture again.
class TextureContentSnapshot {
 public:
  bool Matches(
      const TextureState& texture,
      const TlutState* tlut = nullptr) const;
  void Capture(
      const TextureState& texture,
      const TlutState* tlut = nullptr);

 private:
  bool mValid = false;
  u16 mWidth = 0;
  u16 mHeight = 0;
  GXTexFmt mFormat = GX_TF_I4;
  GXTlutFmt mTlutFormat = GX_TL_IA8;
  u16 mTlutEntries = 0;
  std::vector<u8> mTextureBytes;
  std::vector<u8> mTlutBytes;
};

size_t GetTextureSourceByteSize(const TextureState& texture);
DecodedTlutColor DecodeTlutEntry(
    GXTlutFmt format,
    const u8* canonicalBigEndianBytes);

void ApplyTextureCoordinateGeneration(
    const GlobalState& state,
    std::vector<RenderVertex>& vertices);
void ApplyColorChannels(
    const GlobalState& state,
    std::vector<RenderVertex>& vertices);

u8 ConvertRgbToCopyIntensity(u8 red, u8 green, u8 blue);
u8 ConvertColorToTextureCopyByte(
    u32 destinationFormat,
    u8 red,
    u8 green,
    u8 blue,
    u8 alpha);
void EncodeRgb565TextureCopy(
    const u8* rgba,
    u16 width,
    u16 height,
    u8* encoded);
void EncodeDepthTextureCopy(
    const float* depth,
    u16 width,
    u16 height,
    u32 destinationFormat,
    u8* encoded);

namespace Detail {

enum RenderStateDirty : u32 {
  RenderStateViewport = 1u << 0u,
  RenderStateScissor = 1u << 1u,
  RenderStateDepth = 1u << 2u,
  RenderStateRaster = 1u << 3u,
  RenderStateBlend = 1u << 4u,
  RenderStateAll =
      RenderStateViewport |
      RenderStateScissor |
      RenderStateDepth |
      RenderStateRaster |
      RenderStateBlend,
};

// OpenGL state calls are expensive at -O0 and GX repeats passive register
// values frequently. Cache explicit semantic groups instead of suppressing
// FIFO/BP commands, preserving command-like side effects elsewhere.
class RenderStateCache {
 public:
  u32 Update(
      const ViewportState& viewport,
      const ScissorState& scissor,
      const DepthState& depth,
      const RasterState& raster,
      const BlendState& blend,
      int drawableWidth,
      int drawableHeight);
  void Invalidate() { mValid = false; }

 private:
  ViewportState mViewport = {};
  ScissorState mScissor = {};
  DepthState mDepth = {};
  RasterState mRaster = {};
  BlendState mBlend = {};
  int mDrawableWidth = 0;
  int mDrawableHeight = 0;
  bool mValid = false;
};

constexpr bool ShouldValidateTexture(
    bool textureObjectCreated,
    bool usesTlut,
    u64 cachedTextureRevision,
    u64 textureRevision,
    u64 cachedInvalidationRevision,
    u64 invalidationRevision,
    u64 cachedTlutRevision,
    u64 tlutRevision) {
  return textureObjectCreated ||
         cachedTextureRevision != textureRevision ||
         cachedInvalidationRevision != invalidationRevision ||
         (usesTlut && cachedTlutRevision != tlutRevision);
}

enum class ShaderUniform : size_t {
  Projection,
  ModelView,
  NumTevStages,
  UseTextures,
  StageTextures,
  StageTexCoords,
  StageRasterChannels,
  TevColorInputs,
  TevAlphaInputs,
  TevColorOperations,
  TevAlphaOperations,
  TevOutputRegisters,
  TevSwapSelectors,
  TevSwapTables,
  TevRegisters,
  TevKonstColors,
  TevKonstAlphas,
  AlphaComparison0,
  AlphaReference0,
  AlphaOperation,
  AlphaComparison1,
  AlphaReference1,
  FogType,
  FogOrthographic,
  FogA,
  FogB,
  FogC,
  FogColor,
  FogRangeEnabled,
  FogRangeCenter,
  FogRangeTable,
  FogXScale,
  ZTextureOperation,
  ZTextureFormat,
  ZTextureBias,
  Count,
};

// Uniform locations are stable for the lifetime of a linked GL program.
// Keeping the resolver injectable lets the program-keyed behavior be tested
// without creating an OpenGL context.
class ShaderUniformLocationCache {
 public:
  template <typename Resolver>
  const ShaderUniformLocationCache& Resolve(
      unsigned int program, Resolver&& resolver) {
    if (!mValid || mProgram != program) {
      for (size_t index = 0; index < kNames.size(); ++index) {
        mLocations[index] = resolver(program, kNames[index]);
      }
      mProgram = program;
      mValid = true;
    }
    return *this;
  }

  int operator[](ShaderUniform uniform) const {
    return mLocations[static_cast<size_t>(uniform)];
  }

  void Invalidate() { mValid = false; }

  static constexpr size_t LocationCount() { return kNames.size(); }

 private:
  static constexpr size_t kLocationCount =
      static_cast<size_t>(ShaderUniform::Count);
  inline static constexpr std::array<const char*, kLocationCount> kNames = {
      "u_projection",
      "u_modelview",
      "u_num_tev_stages",
      "u_use_texture[0]",
      "u_stage_texture[0]",
      "u_stage_texcoord[0]",
      "u_stage_raster_channel[0]",
      "u_tev_color_inputs[0]",
      "u_tev_alpha_inputs[0]",
      "u_tev_color_operation[0]",
      "u_tev_alpha_operation[0]",
      "u_tev_output_registers[0]",
      "u_tev_swap_selectors[0]",
      "u_tev_swap_tables[0]",
      "u_tev_registers[0]",
      "u_tev_konst_color[0]",
      "u_tev_konst_alpha[0]",
      "u_alpha_comparison0",
      "u_alpha_reference0",
      "u_alpha_operation",
      "u_alpha_comparison1",
      "u_alpha_reference1",
      "u_fog_type",
      "u_fog_orthographic",
      "u_fog_a",
      "u_fog_b",
      "u_fog_c",
      "u_fog_color",
      "u_fog_range_adjustment_enabled",
      "u_fog_range_adjustment_center",
      "u_fog_range_adjustment[0]",
      "u_fog_x_scale",
      "u_ztexture_operation",
      "u_ztexture_format",
      "u_ztexture_bias",
  };

  std::array<int, kLocationCount> mLocations = {};
  unsigned int mProgram = 0;
  bool mValid = false;
};

}

class GlRenderer {
 public:
  void Draw(std::vector<RenderVertex>& vertices, GXPrimitive primitive);
  void SetDrawableSize(int width, int height);
  void SetShaderProgram(unsigned int program);
  void InvalidateRenderStateCache();

 private:
  void Initialize();

  unsigned int mVertexArray = 0;
  unsigned int mVertexBuffer = 0;
  unsigned int mShaderProgram = 0;
  std::array<unsigned int, 8> mTextures = {};
  std::array<u64, 8> mTextureRevisions = {};
  std::array<u64, 8> mTextureInvalidationRevisions = {};
  std::array<u64, 8> mTextureTlutRevisions = {};
  std::array<TextureContentSnapshot, 8> mTextureSnapshots = {};
  Detail::ShaderUniformLocationCache mUniformLocations;
  Detail::RenderStateCache mRenderStateCache;
  std::vector<RenderVertex> mExpandedVertices;
  int mDrawableWidth = 640;
  int mDrawableHeight = 480;
};

GlRenderer& GetGlRenderer();

}

#endif
