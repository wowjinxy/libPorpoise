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
    bool mHostPackedU32 = false;
    size_t mArraySize = 0;

    bool ResolveRange(
        size_t index, size_t requiredBytes, const u8*& source) const;
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

struct PixelEngineState {
    GXPixelFmt pixelFormat = GX_PF_RGB8_Z24;
    GXZFmt16 zFormat = GX_ZC_LINEAR;
    bool zCompareBeforeTexture = false;
    bool destinationAlphaEnabled = false;
    u8 destinationAlpha = 0;
};

struct ZTextureState {
    GXZTexOp operation = GX_ZT_DISABLE;
    GXTexFmt format = GX_TF_Z24X8;
    u32 bias = 0;
};

struct AlphaCompareState {
    GXCompare comparison0 = GX_ALWAYS;
    u8 reference0 = 0;
    GXAlphaOp operation = GX_AOP_AND;
    GXCompare comparison1 = GX_ALWAYS;
    u8 reference1 = 0;
};

struct FogState {
    GXFogType type = GX_FOG_NONE;
    bool orthographic = false;
    float parameterA = 0.0f;
    u32 parameterBMagnitude = 0;
    u8 parameterBShift = 0;
    float parameterC = 0.0f;
    std::array<float, 3> color = {};
    bool rangeAdjustmentEnabled = false;
    u16 rangeAdjustmentCenter = 0;
    std::array<u16, 10> rangeAdjustmentTable = {};
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
    s32 left = 0;
    s32 top = 0;
    u32 width = 640;
    u32 height = 480;
    s32 offsetX = 0;
    s32 offsetY = 0;
    bool valid = false;
};

struct CopyFilterState {
    // BP 0x53/0x54 store seven six-bit vertical-filter taps. Flipper
    // combines them into one coefficient for each sampled EFB row.
    std::array<u8, 7> coefficients = {0, 0, 21, 22, 21, 0, 0};
    bool halfScale = false;

    std::array<u32, 3> EffectiveCoefficients() const {
        return {
            static_cast<u32>(coefficients[0]) + coefficients[1],
            static_cast<u32>(coefficients[2]) + coefficients[3] +
                coefficients[4],
            static_cast<u32>(coefficients[5]) + coefficients[6],
        };
    }
};

struct TexCoordScaleState {
    // BP stores each texture-coordinate size as (size - 1).
    u16 scaleS = 0;
    u16 scaleT = 0;
    bool biasS = false;
    bool biasT = false;
    bool cylindricalWrapS = false;
    bool cylindricalWrapT = false;
    bool lineOffset = false;
    bool pointOffset = false;
};

struct TextureState {
    enum class SourceEncoding {
        CanonicalBigEndian,
        NativeU16,
    };

    const void* data = nullptr;
    u16 width = 0;
    u16 height = 0;
    GXTexFmt format = GX_TF_I4;
    GXTexWrapMode wrapS = GX_CLAMP;
    GXTexWrapMode wrapT = GX_CLAMP;
    GXTexFilter minFilter = GX_NEAR;
    GXTexFilter magFilter = GX_NEAR;
    bool mipmap = false;
    float minLod = 0.0f;
    float maxLod = 0.0f;
    float lodBias = 0.0f;
    u32 tlutName = GX_TLUT0;
    u64 revision = 0;
    SourceEncoding sourceEncoding = SourceEncoding::CanonicalBigEndian;
};

struct TlutState {
    enum class SourceEncoding {
        CanonicalBigEndian,
        NativeU16,
    };

    const void* data = nullptr;
    GXTlutFmt format = GX_TL_IA8;
    u16 entries = 0;
    u64 revision = 0;
    SourceEncoding sourceEncoding = SourceEncoding::CanonicalBigEndian;
    std::vector<u8> canonicalBytes;

    const void* CanonicalData() const {
        return canonicalBytes.empty() ? data : canonicalBytes.data();
    }
};

struct TexCoordGenState {
    GXTexGenSrc source = GX_TG_TEX0;
    GXTexGenType function = GX_TG_MTX2x4;
    u8 matrixId = GX_IDENTITY;
    u8 postMatrixId = GX_PTIDENTITY;
    bool normalize = false;
    u8 embossSource = 0;
    u8 embossLight = 0;
};

enum class TevColorMode {
    PassColor,
    ReplaceTexture,
    Modulate,
    CompareTextureRgb8EqualZero,
};

struct TevStageState {
    u8 textureMap = 0;
    u8 textureCoordinate = 0;
    u8 rasterChannel = 0;
    u8 rasterSwapTable = GX_TEV_SWAP0;
    u8 textureSwapTable = GX_TEV_SWAP0;
    u8 konstColorSelection = GX_TEV_KCSEL_1_4;
    u8 konstAlphaSelection = GX_TEV_KASEL_1;
    bool textureEnabled = false;
    TevColorMode colorMode = TevColorMode::PassColor;
    std::array<u8, 4> colorInputs = {
        GX_CC_ZERO,
        GX_CC_ZERO,
        GX_CC_ZERO,
        GX_CC_RASC,
    };
    std::array<u8, 4> alphaInputs = {
        GX_CA_ZERO,
        GX_CA_ZERO,
        GX_CA_ZERO,
        GX_CA_RASA,
    };
    GXTevOp colorOperation = GX_TEV_ADD;
    GXTevOp alphaOperation = GX_TEV_ADD;
    GXTevBias colorBias = GX_TB_ZERO;
    GXTevBias alphaBias = GX_TB_ZERO;
    GXTevScale colorScale = GX_CS_SCALE_1;
    GXTevScale alphaScale = GX_CS_SCALE_1;
    bool colorClamp = true;
    bool alphaClamp = true;
    GXTevRegID colorOutput = GX_TEVPREV;
    GXTevRegID alphaOutput = GX_TEVPREV;
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
  const std::array<float, 16>& GetPositionMatrix(size_t index) const;
  const std::array<float, 16>& GetNormalMatrix() const;
  const std::array<float, 16>& GetNormalMatrix(size_t index) const;
  const std::array<float, 16>& GetProjectionMatrix() const;
  const std::array<float, 16>& GetTextureMatrix(size_t index) const;
  const std::array<float, 16>& GetPostTextureMatrix(size_t index) const;
  const std::array<float, 6>& GetViewportTransform() const;
  const ViewportState& GetViewportState() const;
  const ScissorState& GetScissorState() const;
  const CopyFilterState& GetCopyFilterState() const;
  const BlendState& GetBlendState() const;
  const DepthState& GetDepthState() const;
  const PixelEngineState& GetPixelEngineState() const;
  const ZTextureState& GetZTextureState() const;
  const AlphaCompareState& GetAlphaCompareState() const;
  const FogState& GetFogState() const;
  const RasterState& GetRasterState() const;
  const ChannelState& GetChannelState(size_t index) const;
  const LightState& GetLightState(size_t index) const;
  const TextureState& GetTextureState(size_t index) const;
  const TlutState& GetTlutState(size_t index) const;
  u64 GetTextureInvalidationRevision() const;
  u64 GetUniformStateRevision() const;
  const TexCoordGenState& GetTexCoordGenState(size_t index) const;
  const TexCoordScaleState& GetTexCoordScaleState(size_t index) const;
  const std::array<float, 16>& GetTexCoordGenMatrix(size_t index) const;
  const std::array<float, 16>& GetTexCoordGenPostMatrix(size_t index) const;
  const TevStageState& GetTevStageState(size_t index) const;
  const std::array<u8, 4>& GetTevSwapTable(size_t index) const;
  const std::array<float, 4>& GetTevColor(size_t index) const;
  std::array<float, 4> GetTevKonstColor(size_t stage) const;
  float GetTevKonstAlpha(size_t stage) const;
  size_t GetNumColorChannels() const;
  size_t GetNumTexGens() const;
  size_t GetNumTevStages() const;
  const std::array<float, 4>& GetCopyClearColor() const;
  float GetCopyClearDepth() const;
  bool HasViewportTransform() const;
  bool ConsumeCopyClearRequest();

  void Reset();
  u32 SetBpRegister(u32 registerValue);
  void SetCurrentPrimitive(GXPrimitive primitive);
  void SetCurrentPositionMatrix(u32 matrixId);
  void SetCurrentVertexFormat(GXVtxFmt format);
  void SetVertexArray(GXAttr attr, VertexArray array);
  void SetVertexDescriptor(GXAttr attr, GXAttrType descType);
  void SetVertexFormatComponents(GXVtxFmt formatIndex, GXAttr attrIndex, GXCompCnt component);
  void SetVertexFormatDataType(GXVtxFmt formatIndex, GXAttr attrIndex, GXCompType dataType);
  void SetVertexFormatFraction(GXVtxFmt formatIndex, GXAttr attrIndex, u8 fraction);
  void LoadTexture(size_t index, const TextureState& texture);
  void LoadTlut(size_t index, const TlutState& tlut);
  void SetXfData(u32 address, const u8* data, size_t wordCount);

 private:
  static std::array<float, 16> IdentityMatrix();
  static float WordToFloat(u32 word);
  void RefreshPositionMatrices(u32 firstAddress, u32 endAddress);
  void RefreshNormalMatrices(u32 firstAddress, u32 endAddress);
  void RefreshTextureMatrices(u32 firstAddress, u32 endAddress);
  void RefreshPostTextureMatrices(u32 firstAddress, u32 endAddress);
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
  std::array<bool, 0x1100> mXfWordWritten = {};
  std::array<std::array<float, 16>, 10> mPositionMatrices = {};
  std::array<bool, 10> mPositionMatrixValid = {};
  std::array<std::array<float, 16>, 10> mNormalMatrices = {};
  std::array<bool, 10> mNormalMatrixValid = {};
  std::array<std::array<float, 16>, 10> mTextureMatrices = {};
  std::array<bool, 10> mTextureMatrixValid = {};
  std::array<bool, 10> mTextureMatrix2x4 = {};
  std::array<std::array<float, 16>, 20> mPostTextureMatrices = {};
  std::array<bool, 20> mPostTextureMatrixValid = {};
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
  CopyFilterState mCopyFilterState = {};
  BlendState mBlendState = {};
  DepthState mDepthState = {};
  PixelEngineState mPixelEngineState = {};
  ZTextureState mZTextureState = {};
  AlphaCompareState mAlphaCompareState = {};
  FogState mFogState = {};
  RasterState mRasterState = {};
  std::array<ChannelState, 2> mChannels = {};
  std::array<LightState, 8> mLights = {};
  std::array<TextureState, 8> mTextures = {};
  std::array<TlutState, 20> mTluts = {};
  std::array<TevStageState, 16> mTevStages = {};
  std::array<TexCoordScaleState, 8> mTexCoordScales = {};
  std::array<std::array<u8, 4>, 4> mTevSwapTables = {};
  std::array<std::array<float, 4>, 4> mTevColors = {};
  std::array<std::array<float, 4>, 4> mTevKonstColors = {};
  std::array<u32, 0x100> mBpRegisters = {};
  std::array<bool, 0x100> mBpRegisterWritten = {};
  u32 mBpWriteMask = 0x00ffffffu;
  size_t mNumColorChannels = 0;
  size_t mNumTexGens = 1;
  size_t mNumTevStages = 1;
  u64 mTextureRevision = 0;
  u64 mTextureInvalidationRevision = 0;
  u64 mUniformStateRevision = 0;
  std::array<float, 4> mCopyClearColor = {};
  float mCopyClearDepth = 1.0f;
  bool mCopyClearRequested = false;
};

void InitGlobalState();
GlobalState& GetGlobalState();

}


#endif
