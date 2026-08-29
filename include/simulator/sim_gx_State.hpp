#ifndef LIBPORPOISE_SIM_GX_STATE_HPP
#define LIBPORPOISE_SIM_GX_STATE_HPP

#include "dolphin/gx/GXEnum.h"
#include <array>
#include <cstddef>
#include <unordered_set>
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
    GXAttr attribute;
    void * mArrayPtr = nullptr;
    int mStride = 0;
};

struct Tlut {
  u16 mNumEntries;
  GXTlutFmt mFormat;
  void * mSourceAddress;
};

// NOTE: this must match exactly with the struct in fragment.glsl
// otherwise the shader will not work!
// Make sure to update the values in the shader if you change them here!
struct TevStageConfig {
  GXTevMode mMode;
  GXTevOp mColorOperation;
  GXTevOp mAlphaOperation;
  u32 pad1;
  GXTevColorArg mColorArgs[4];
  GXTevAlphaArg mAlphaArgs[4];
  GXTevRegID mOutReg;
  GXTevClampMode mClampMode;
  GXTevBias mBias;
  GXTevScale mScale;
  GXTexCoordID mTexCoordId;
  u32 pad2;
  u32 pad3;
  u32 pad4;
};

struct TexGenConfig {
  u32 mMatrixId;
  GXTexGenType mType;
};

class GlobalState {
 public:
  GlobalState();
  ~GlobalState() = default;

  static GlobalState& GetInstance();

  size_t GetDescriptorSize(GXAttrType descriptorType, GXCompType dataType, bool isColorType = false);
  size_t GetNumBytesPerVertex();
  static inline size_t GetNumMtxIdxComponents(GXCompCnt compType) {
    return 1;
  };
  static inline size_t GetNumPositionComponents(GXCompCnt compType) {
    switch(compType) {
        case GX_POS_XY:
            return 2;
        default:
        case GX_POS_XYZ:
            return 3;
    }
  };

  static inline size_t GetNumNormalComponents(GXCompCnt compType) {
    //Normal always has 3 components
    return 3;
  };

  static inline size_t GetNumColorComponents(GXCompCnt compType) {
    return 1;
  };

  static inline size_t GetNumTexCoordComponents(GXCompCnt compType) {
    switch(compType) {
        case GX_TEX_S:
            return 1;
        default:
        case GX_TEX_ST:
            return 2;
    }
  };

  static inline size_t GetNumNBTComponents(GXCompCnt compType) {
    //NBT always has 3 components
    //TODO: verify this
    return 3;
  };

  inline u32 GetBpRegCache(u8 regId) const { return mBpRegCache[regId]; };
  inline GXPrimitive GetCurrentPrimitive() const {return mCurrentPrimitive;};
  inline GXAttrType GetVertexDescriptor(GXAttr attr) {return mVertexDescriptors[attr];};
  inline const VertexFormat& GetCurrentVertexFormat() {return mVertexFormats[mCurrentVertexFormat];};
  inline const VertexArray& GetVertexArray(GXAttr attr) {return mVertexArrays[attr];};
  inline const VertexFormat& GetVertexFormat(GXVtxFmt formatIdx) {return mVertexFormats[formatIdx];};
  const std::array<float, 16>& GetPositionMatrix() const;
  const std::array<float, 16>& GetProjectionMatrix() const;
  const std::array<float, 16>& GetTextureMatrix(int id) const;
  inline u8 GetNumTexGens() const { return mNumTexGens; };
  inline u8 GetNumChannels() const { return mNumChannels; };
  inline u8 GetNumTevStages() const { return mNumTevStages; };
  inline GXCullMode GetCullMode() const { return mCullMode; };
  inline TevStageConfig& GetTevStageConfig(u8 stage) { return mTevStages[stage]; };
  inline TevStageConfig * GetTevStageConfigArray() { return mTevStages.data(); };
  inline GXTexMapID* GetTevTexMapArray() {return mTevTexMaps.data(); };
  inline TexGenConfig* GetTexGenArray() {return mTexGenConfigs.data(); };
  inline float* GetInitialTevColorsArray() {return mInitialTevColors[0].data(); };
  inline std::array<float, 4> GetTevColor(u8 reg) const {return mInitialTevColors[reg];};
  inline GXTexObjPriv& GetLoadedTexObj(u8 texMap) { return mLoadedTexObjs[texMap]; };
  inline GXTexRegionPriv& GetLoadedTexRegion(u8 texMap) { return mLoadedTexRegions[texMap]; };
  inline Tlut& GetLoadedTlut(u8 tlutName) { return mLoadedTluts[tlutName]; };
  inline u8 GetTlutAssignment(GXTexMapID texMap) { return mTlutAssignments[texMap]; };
  inline bool GetIsTextureDirty() { return mTextureDirty;};
  inline void SetTextureDirty(bool dirty) { mTextureDirty = dirty; };

  void Reset();
  inline void SetBpRegCache(u8 regId, u32 value) {
    mBpRegCache[regId] = value;
  }
  inline void SetCurrentPrimitive(GXPrimitive primitive) {mCurrentPrimitive = primitive;};
  inline void SetCurrentPositionMatrix(u32 matrixId) {
    const size_t slot = static_cast<size_t>(matrixId / 3);
    if (slot < mPositionMatrices.size()) {
        mCurrentPositionMatrix = slot;
    }
  };
  inline void SetCurrentVertexFormat(GXVtxFmt format) {mCurrentVertexFormat = format;};
  inline void SetVertexArray(GXAttr attr, VertexArray array) {mVertexArrays[attr] = array;};
  inline void SetVertexDescriptor(GXAttr attr, GXAttrType descType) {mVertexDescriptors[attr] = descType;};
  inline void SetVertexFormatComponents(GXVtxFmt formatIndex, GXAttr attrIndex, GXCompCnt component) {
    mVertexFormats[formatIndex].mAttributes[attrIndex].mComponents = component;
  };
  inline void SetVertexFormatDataType(GXVtxFmt formatIndex, GXAttr attrIndex, GXCompType dataType) {
    mVertexFormats[formatIndex].mAttributes[attrIndex].mDataType = dataType;
  };
  inline void SetVertexFormatFraction(GXVtxFmt formatIndex, GXAttr attrIndex, u8 fraction) {
    mVertexFormats[formatIndex].mAttributes[attrIndex].mFraction = fraction;
  };
  void SetXfData(u32 address, const u8* data, size_t wordCount);
  inline void SetNumTexGens(u8 numTexGens) { mNumTexGens = numTexGens; };
  inline void SetNumChannels(u8 numChannels) { mNumChannels = numChannels; };
  inline void SetNumTevStages(u8 numTevStages) { mNumTevStages = numTevStages; };
  inline void SetCullMode(GXCullMode cullMode) { mCullMode = cullMode; };
  inline void SetTevTexMap(u8 tevStage, GXTexMapID texMap) { mTevTexMaps[tevStage] = texMap; };
  inline void SetTlutAssignment(GXTexMapID texMap, u8 tlutName) { mTlutAssignments[texMap] = tlutName; };
  void SetTevColor(u8 reg, std::array<float, 4>& color);
  void AddNativeEndianDisplayList(void * displayListPtr);
  bool IsDisplayListNativeEndian(void * displayListPtr);

 private:
  static std::array<float, 16> IdentityMatrix();
  static float WordToFloat(u32 word);
  void RefreshPositionMatrices(u32 firstAddress, u32 endAddress);
  void RefreshTextureMatrices(u32 firstAddress, u32 endAddress);

  GXVtxFmt mCurrentVertexFormat = GX_VTXFMT0;
  GXPrimitive mCurrentPrimitive = GX_TRIANGLES;
  size_t mCurrentPositionMatrix = 0;
  std::array<GXAttrType, GX_VA_MAX_ATTR> mVertexDescriptors = {};
  std::array<VertexFormat, GX_MAX_VTXFMT> mVertexFormats = {};
  std::array<VertexArray, GX_VA_MAX_ATTR> mVertexArrays = {};
  std::array<u32, 0x1100> mXfMemory = {};
  std::array<std::array<float, 16>, 10> mPositionMatrices = {};
  std::array<std::array<float, 16>, 10> mTextureMatrices = {};
  std::array<bool, 10> mPositionMatrixValid = {};
  std::array<bool, 10> mTextureMatrixValid = {};
  std::array<float, 16> mProjectionMatrix = {};
  std::array<std::array<float, 4>, 4> mInitialTevColors = {};
  bool mProjectionMatrixValid = false;
  std::array<u32, 0x100> mBpRegCache = {};
  u8 mNumTexGens = 0;
  u8 mNumChannels = 0;
  u8 mNumTevStages = 0;
  GXCullMode mCullMode = GX_CULL_BACK;
  std::array<TevStageConfig, GX_MAX_TEVSTAGE> mTevStages = {};
  std::array<GXTexMapID, GX_MAX_TEVSTAGE> mTevTexMaps;
  std::array<TexGenConfig, GX_MAX_TEXCOORD> mTexGenConfigs = {};
  std::unordered_set<void*> mNativeEndianDisplayLists;
  std::array<GXTexObjPriv, GX_MAX_TEXMAP> mLoadedTexObjs = {};
  std::array<GXTexRegionPriv, GX_MAX_TEXMAP> mLoadedTexRegions = {};
  std::array<Tlut, GX_MAX_TLUT> mLoadedTluts = {};
  std::array<u8, GX_MAX_TEXMAP> mTlutAssignments = {};
  bool mTextureDirty;
};

void InitGlobalState();
GlobalState& GetGlobalState();

}


#endif
