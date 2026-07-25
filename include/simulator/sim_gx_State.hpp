#ifndef LIBPORPOISE_SIM_GX_STATE_HPP
#define LIBPORPOISE_SIM_GX_STATE_HPP

#include <array>
#include <cstddef>
#include <vector>

#include <dolphin/types.h>
#include <dolphin/gx/GXAttr.h>

namespace SIM::GX {

struct VertexAttributes {
    GXCompCnt mComponents = GX_COMPCNT_NULL;
    GXCompType mDataType = GX_U8;
    u8 mFraction = 0;
};

struct VertexFormat {
    std::array<VertexAttributes, GX_VA_MAX_ATTR> mAttributes;
};

struct VertexArray {
    void * mArrayPtr = nullptr;
    int mStride = 0;
};

struct BlendState {
    GXBlendMode mode = GX_BM_NONE;
    GXBlendFactor sourceFactor = GX_BL_ONE;
    GXBlendFactor destinationFactor = GX_BL_ZERO;
    GXLogicOp logicOperation = GX_LO_CLEAR;
    bool colorUpdateEnabled = true;
    bool alphaUpdateEnabled = true;
    bool ditherEnabled = true;
};

struct DepthState {
    bool compareEnabled = true;
    GXCompare function = GX_LEQUAL;
    bool updateEnabled = true;
};

struct RasterState {
    GXCullMode cullMode = GX_CULL_NONE;
    float lineWidth = 1.0f;
    float pointSize = 1.0f;
};

struct ChannelControlState {
    bool lightingEnabled = false;
    GXColorSrc ambientSource = GX_SRC_REG;
    GXColorSrc materialSource = GX_SRC_VTX;
    u8 lightMask = 0;
    GXDiffuseFn diffuseFunction = GX_DF_NONE;
    GXAttnFn attenuationFunction = GX_AF_NONE;
};

struct ChannelState {
    std::array<float, 4> ambientColor = {};
    std::array<float, 4> materialColor = {1.0f, 1.0f, 1.0f, 1.0f};
    ChannelControlState colorControl = {};
    ChannelControlState alphaControl = {};
};

struct LightState {
    std::array<float, 4> color = {};
    std::array<float, 3> cosineAttenuation = {};
    std::array<float, 3> distanceAttenuation = {};
    std::array<float, 3> position = {};
    std::array<float, 3> direction = {};
    bool valid = false;
};

struct ViewportState {
    float left = 0.0f;
    float top = 0.0f;
    float width = 640.0f;
    float height = 480.0f;
    float referenceWidth = 640.0f;
    float referenceHeight = 480.0f;
    bool valid = false;
};

struct ScissorState {
    u32 left = 0;
    u32 top = 0;
    u32 width = 640;
    u32 height = 480;
    bool valid = false;
};

struct TextureState {
    const void* data = nullptr;
    u16 width = 0;
    u16 height = 0;
    GXTexFmt format = GX_TF_I4;
    GXTexWrapMode wrapS = GX_CLAMP;
    GXTexWrapMode wrapT = GX_CLAMP;
    GXTexFilter minFilter = GX_NEAR;
    GXTexFilter magFilter = GX_NEAR;
    u64 revision = 0;
};

struct TexCoordGenState {
    GXTexGenSrc source = GX_TG_TEX0;
    u8 matrixId = GX_IDENTITY;
};

enum class TevColorMode {
    PassColor,
    ReplaceTexture,
    Modulate,
};

struct TevStageState {
    u8 textureMap = 0;
    u8 textureCoordinate = 0;
    bool textureEnabled = false;
    TevColorMode colorMode = TevColorMode::PassColor;
};

class GlobalState {
 public:
  GlobalState();
  ~GlobalState() = default;

  static GlobalState& GetInstance();

  size_t GetDescriptorSize(GXAttrType descriptorType, GXCompType dataType, bool isColorType = false) const;
  size_t GetVertexAttributeInputSize(GXAttr attr) const;
  size_t GetNumBytesPerVertex() const;
  size_t GetNumPositionComponents(GXCompCnt compType) const;

  GXPrimitive GetCurrentPrimitive() const;
  GXAttrType GetVertexDescriptor(GXAttr attr) const;
  const VertexFormat& GetCurrentVertexFormat() const;
  const VertexArray& GetVertexArray(GXAttr attr) const;
  const VertexFormat& GetVertexFormat(GXVtxFmt formatIdx) const;
  const std::array<float, 16>& GetPositionMatrix() const;
  const std::array<float, 16>& GetProjectionMatrix() const;
  const std::array<float, 16>& GetTextureMatrix(size_t index) const;
  const std::array<float, 6>& GetViewportTransform() const;
  const ViewportState& GetViewportState() const;
  const ScissorState& GetScissorState() const;
  const BlendState& GetBlendState() const;
  const DepthState& GetDepthState() const;
  const RasterState& GetRasterState() const;
  const ChannelState& GetChannelState(size_t index) const;
  const LightState& GetLightState(size_t index) const;
  const TextureState& GetTextureState(size_t index) const;
  const TexCoordGenState& GetTexCoordGenState(size_t index) const;
  const std::array<float, 16>& GetTexCoordGenMatrix(size_t index) const;
  const TevStageState& GetTevStageState(size_t index) const;
  size_t GetNumTevStages() const;
  const std::array<float, 4>& GetCopyClearColor() const;
  float GetCopyClearDepth() const;
  bool HasViewportTransform() const;
  bool ConsumeCopyClearRequest();

  void Reset();
  void SetBpRegister(u32 registerValue);
  void SetCurrentPrimitive(GXPrimitive primitive);
  void SetCurrentPositionMatrix(u32 matrixId);
  void SetCurrentVertexFormat(GXVtxFmt format);
  void SetVertexArray(GXAttr attr, VertexArray array);
  void SetVertexDescriptor(GXAttr attr, GXAttrType descType);
  void SetVertexFormatComponents(GXVtxFmt formatIndex, GXAttr attrIndex, GXCompCnt component);
  void SetVertexFormatDataType(GXVtxFmt formatIndex, GXAttr attrIndex, GXCompType dataType);
  void SetVertexFormatFraction(GXVtxFmt formatIndex, GXAttr attrIndex, u8 fraction);
  void LoadTexture(size_t index, const TextureState& texture);
  void SetXfData(u32 address, const u8* data, size_t wordCount);

 private:
  static std::array<float, 16> IdentityMatrix();
  static float WordToFloat(u32 word);
  void RefreshPositionMatrices(u32 firstAddress, u32 endAddress);
  void RefreshTextureMatrices(u32 firstAddress, u32 endAddress);
  void RefreshTexCoordGenState(u32 firstAddress, u32 endAddress);
  void RefreshProjectionMatrix(u32 firstAddress, u32 endAddress);
  void RefreshViewportTransform(u32 firstAddress, u32 endAddress);
  void RefreshChannelState(u32 firstAddress, u32 endAddress);
  void RefreshLightState(u32 firstAddress, u32 endAddress);

  GXVtxFmt mCurrentVertexFormat = GX_VTXFMT0;
  GXPrimitive mCurrentPrimitive = GX_TRIANGLES;
  size_t mCurrentPositionMatrix = 0;
  std::array<GXAttrType, GX_VA_MAX_ATTR> mVertexDescriptors = {};
  std::array<VertexFormat, GX_MAX_VTXFMT> mVertexFormats = {};
  std::array<VertexArray, GX_VA_MAX_ATTR> mVertexArrays = {};
  std::array<u32, 0x1100> mXfMemory = {};
  std::array<std::array<float, 16>, 10> mPositionMatrices = {};
  std::array<bool, 10> mPositionMatrixValid = {};
  std::array<std::array<float, 16>, 10> mTextureMatrices = {};
  std::array<bool, 10> mTextureMatrixValid = {};
  std::array<TexCoordGenState, 8> mTexCoordGens = {};
  std::array<float, 16> mProjectionMatrix = {};
  bool mProjectionMatrixValid = false;
  std::array<float, 6> mViewportTransform = {};
  bool mViewportTransformValid = false;
  ViewportState mViewportState = {};
  ScissorState mScissorState = {};
  u32 mScissorTopLeft = 0;
  u32 mScissorBottomRight = 0;
  u32 mCopySourceLeft = 0;
  u32 mCopySourceTop = 0;
  u32 mCopySourceWidth = 640;
  u32 mCopySourceHeight = 480;
  bool mCopySourceValid = false;
  BlendState mBlendState = {};
  DepthState mDepthState = {};
  RasterState mRasterState = {};
  std::array<ChannelState, 2> mChannels = {};
  std::array<LightState, 8> mLights = {};
  std::array<TextureState, 8> mTextures = {};
  std::array<TevStageState, 16> mTevStages = {};
  size_t mNumTevStages = 1;
  u64 mTextureRevision = 0;
  std::array<float, 4> mCopyClearColor = {};
  float mCopyClearDepth = 1.0f;
  bool mCopyClearRequested = false;
};

void InitGlobalState();
GlobalState& GetGlobalState();

}


#endif
