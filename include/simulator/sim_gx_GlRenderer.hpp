#ifndef LIBPORPOISE_SIM_GX_GLRENDERER_HPP
#define LIBPORPOISE_SIM_GX_GLRENDERER_HPP

#include <array>
#include <cstddef>
#include <unordered_map>
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

struct TextureMipLevelLayout {
  size_t offset = 0;
  size_t byteSize = 0;
  u16 width = 0;
  u16 height = 0;
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
  bool CaptureCanonical(
      const TextureState& texture,
      std::vector<u8>&& canonicalTextureBytes,
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

size_t GetTextureMipLevelCount(const TextureState& texture);
bool GetTextureMipLevelLayout(
    const TextureState& texture,
    size_t level,
    TextureMipLevelLayout& layout);
size_t GetTextureSourceByteSize(const TextureState& texture);
bool CopyCanonicalTextureBytes(
    const TextureState& texture,
    std::vector<u8>& canonicalBytes);
void NotifyTextureCopyDestinationWrite(GlobalState& state);
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
bool EncodeColorTextureCopy(
    const u8* rgba,
    u16 width,
    u16 height,
    u32 destinationFormat,
    u8* encoded);
bool EncodeDepthTextureCopyBytes(
    const u8* depthBytes,
    u16 width,
    u16 height,
    u32 destinationFormat,
    u8* encoded);
void EncodeDepthTextureCopy(
    const float* depth,
    u16 width,
    u16 height,
    u32 destinationFormat,
    u8* encoded);

namespace Detail {

enum class TextureMipmapFilter {
  None,
  Nearest,
  Linear,
};

struct TextureFilterSelection {
  bool linearTexels = false;
  TextureMipmapFilter mipmapFilter = TextureMipmapFilter::None;
};

TextureFilterSelection SelectTextureFilter(GXTexFilter filter);
bool ResampleOpenGlFramebufferRgba(
    const u8* framebuffer,
    size_t framebufferWidth,
    size_t framebufferHeight,
    float sourceLeft,
    float sourceTop,
    float sourceWidth,
    float sourceHeight,
    size_t destinationWidth,
    size_t destinationHeight,
    std::vector<u8>& rgba);
bool FilterTextureCopyRgba(
    const u8* framebuffer,
    size_t framebufferWidth,
    size_t framebufferHeight,
    float sourceLeft,
    float sourceTop,
    float sourceWidth,
    float sourceHeight,
    size_t destinationWidth,
    size_t destinationHeight,
    const CopyFilterState& filter,
    std::vector<u8>& rgba);
bool FilterTextureCopyDepth(
    const float* framebuffer,
    size_t framebufferWidth,
    size_t framebufferHeight,
    float sourceLeft,
    float sourceTop,
    float sourceWidth,
    float sourceHeight,
    size_t destinationWidth,
    size_t destinationHeight,
    const CopyFilterState& filter,
    std::vector<u8>& depthBytes);
bool DecodeCanonicalTextureMipLevelToRgba(
    const TextureState& texture,
    const u8* canonicalBytes,
    size_t canonicalByteSize,
    size_t level,
    const std::vector<u8>& palette,
    std::vector<u8>& rgba);
bool DecodeTextureToRgba(
    const TextureState& texture,
    const std::vector<u8>& palette,
    std::vector<u8>& rgba);

// Image identity deliberately excludes GX sampler parameters. One decoded
// OpenGL image can therefore be reused by different materials and texture-map
// units while a sampler object supplies each binding's wrap/filter/LOD state.
struct TextureSourceKey {
  const void* data = nullptr;
  u16 width = 0;
  u16 height = 0;
  GXTexFmt format = GX_TF_I4;
  bool mipmap = false;
  size_t mipLevelCount = 0;
  TextureState::SourceEncoding sourceEncoding =
      TextureState::SourceEncoding::CanonicalBigEndian;
  bool usesTlut = false;
  u32 tlutName = 0;
  GXTlutFmt tlutFormat = GX_TL_IA8;
  u16 tlutEntries = 0;
  u64 tlutRevision = 0;
  TlutState::SourceEncoding tlutSourceEncoding =
      TlutState::SourceEncoding::CanonicalBigEndian;

  bool operator==(const TextureSourceKey& other) const noexcept;
};

struct TextureSourceKeyHash {
  size_t operator()(const TextureSourceKey& key) const noexcept;
};

TextureSourceKey MakeTextureSourceKey(
    const TextureState& texture,
    const TlutState* tlut = nullptr);
size_t GetDecodedTextureByteSize(const TextureState& texture);

struct TextureCacheEntry {
  unsigned int texture = 0;
  TextureContentSnapshot snapshot;
  u64 validatedInvalidationRevision = 0;
  u64 lastUsedSerial = 0;
  size_t decodedByteSize = 0;
};

struct DrawableScissorRect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

DrawableScissorRect ComputeDrawableScissor(
    const ViewportState& viewport,
    const ScissorState& scissor,
    int drawableWidth,
    int drawableHeight);

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
  StageTexCoordScales,
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

constexpr u64 ShaderUniformMask(ShaderUniform uniform) {
  return u64{1} << static_cast<size_t>(uniform);
}

constexpr u64 AllShaderUniformsMask() {
  static_assert(static_cast<size_t>(ShaderUniform::Count) < 64u);
  return
      (u64{1} << static_cast<size_t>(ShaderUniform::Count)) - u64{1};
}

constexpr bool IsShaderUniformDirty(u64 dirty, ShaderUniform uniform) {
  return (dirty & ShaderUniformMask(uniform)) != 0u;
}

// Host-native values exactly as consumed by the GLSL program. Comparing this
// payload avoids repeating driver calls when GX state is unchanged while
// leaving FIFO parsing and its explicit big-endian decode boundaries alone.
struct ShaderUniformValues {
  static constexpr size_t MaxTevStages = 16u;

  std::array<float, 16> projection = {};
  std::array<float, 16> modelView = {};
  int numTevStages = 0;
  std::array<int, MaxTevStages> useTextures = {};
  std::array<int, MaxTevStages> stageTextures = {};
  std::array<int, MaxTevStages> stageTexCoords = {};
  std::array<float, MaxTevStages * 2u> stageTexCoordScales = {};
  std::array<int, MaxTevStages> stageRasterChannels = {};
  std::array<int, MaxTevStages * 4u> tevColorInputs = {};
  std::array<int, MaxTevStages * 4u> tevAlphaInputs = {};
  std::array<int, MaxTevStages * 4u> tevColorOperations = {};
  std::array<int, MaxTevStages * 4u> tevAlphaOperations = {};
  std::array<int, MaxTevStages * 2u> tevOutputRegisters = {};
  std::array<int, MaxTevStages * 2u> tevSwapSelectors = {};
  std::array<int, 4u * 4u> tevSwapTables = {};
  std::array<float, 4u * 4u> tevRegisters = {};
  std::array<float, MaxTevStages * 4u> tevKonstColors = {};
  std::array<float, MaxTevStages> tevKonstAlphas = {};
  int alphaComparison0 = 0;
  int alphaReference0 = 0;
  int alphaOperation = 0;
  int alphaComparison1 = 0;
  int alphaReference1 = 0;
  int fogType = 0;
  int fogOrthographic = 0;
  float fogA = 0.0f;
  float fogB = 0.0f;
  float fogC = 0.0f;
  std::array<float, 3> fogColor = {};
  int fogRangeEnabled = 0;
  float fogRangeCenter = 0.0f;
  std::array<float, 10> fogRangeTable = {};
  float fogXScale = 1.0f;
  int zTextureOperation = 0;
  int zTextureFormat = 0;
  u32 zTextureBias = 0;
};

class ShaderUniformValueCache {
 public:
  u64 Update(const ShaderUniformValues& values);
  void Invalidate() { mValid = false; }

 private:
  ShaderUniformValues mValues = {};
  bool mValid = false;
};

struct VertexStreamAllocation {
  size_t pageIndex = 0;
  size_t firstVertex = 0;
  bool pageChanged = false;
};

// Pure page allocator for a persistently mapped vertex ring. The renderer
// fences the old page and waits for the returned page before writing whenever
// pageChanged is true.
class VertexStreamRing {
 public:
  VertexStreamRing(size_t pageCapacity, size_t pageCount)
      : mPageCapacity(pageCapacity), mPageCount(pageCount) {}

  bool Allocate(size_t vertexCount, VertexStreamAllocation& allocation) {
    if (vertexCount == 0u || vertexCount > mPageCapacity ||
        mPageCapacity == 0u || mPageCount == 0u) {
      return false;
    }

    allocation.pageChanged = false;
    if (mPageOffset + vertexCount > mPageCapacity) {
      mPageIndex = (mPageIndex + 1u) % mPageCount;
      mPageOffset = 0u;
      allocation.pageChanged = true;
    }
    allocation.pageIndex = mPageIndex;
    allocation.firstVertex =
        mPageIndex * mPageCapacity + mPageOffset;
    mPageOffset += vertexCount;
    return true;
  }

 private:
  size_t mPageCapacity = 0u;
  size_t mPageCount = 0u;
  size_t mPageIndex = 0u;
  size_t mPageOffset = 0u;
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
      "u_stage_texcoord_scale[0]",
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
  void InvalidateShaderProgramCache();
  void InvalidateRenderStateCache();

 private:
  void Initialize();
  void TrimTextureCache(size_t incomingDecodedBytes);
  void DrawPersistentVertices(
      const std::vector<RenderVertex>& vertices,
      GXPrimitive primitive);
  void DrawOverflowVertices(
      const std::vector<RenderVertex>& vertices,
      GXPrimitive primitive);
  void AdvancePersistentVertexPage(size_t pageIndex);

  static constexpr size_t VertexStreamPageCount = 3u;
  static constexpr size_t VertexStreamPageCapacity = 65535u;
  static constexpr size_t TextureMapCount = 8u;
  static constexpr size_t MaximumTextureCacheEntries = 4096u;
  static constexpr size_t MaximumTextureCacheBytes = 512u * 1024u * 1024u;

  unsigned int mVertexArray = 0;
  unsigned int mVertexBuffer = 0;
  unsigned int mOverflowVertexArray = 0;
  unsigned int mOverflowVertexBuffer = 0;
  unsigned int mShaderProgram = 0;
  std::unordered_map<
      Detail::TextureSourceKey,
      Detail::TextureCacheEntry,
      Detail::TextureSourceKeyHash> mTextureCache;
  size_t mTextureCacheDecodedBytes = 0u;
  u64 mTextureUseSerial = 0u;
  Detail::ShaderUniformLocationCache mUniformLocations;
  Detail::ShaderUniformValueCache mUniformValues;
  Detail::ShaderUniformValues mUniformScratch = {};
  Detail::RenderStateCache mRenderStateCache;
  std::vector<RenderVertex> mExpandedVertices;
  Detail::VertexStreamRing mVertexStreamRing = {
      VertexStreamPageCapacity,
      VertexStreamPageCount,
  };
  std::array<void*, VertexStreamPageCount> mVertexStreamFences = {};
  u8* mMappedVertexBytes = nullptr;
  size_t mActiveVertexStreamPage = 0u;
  bool mPersistentVertexStream = false;
  bool mVertexStreamPageHasDraws = false;
  u64 mUniformStateRevision = 0;
  bool mUniformStateRevisionValid = false;
  int mUniformDrawableWidth = 0;
  int mUniformDrawableHeight = 0;
  int mDrawableWidth = 640;
  int mDrawableHeight = 480;
};

GlRenderer& GetGlRenderer();

}

#endif
