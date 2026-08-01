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

class GlobalState {
 public:
  GlobalState();
  ~GlobalState() = default;

  static GlobalState& GetInstance();

  size_t GetDescriptorSize(GXAttrType descriptorType, GXCompType dataType, bool isColorType = false);
  size_t GetNumBytesPerVertex();
  static inline size_t GetNumPositionComponents(GXCompCnt compType) {
    switch(compType) {
        case GX_POS_XY:
            return 2;
        default:
        case GX_POS_XYZ:
            return 3;
    }
  };

  inline GXPrimitive GetCurrentPrimitive() const {return mCurrentPrimitive;};
  inline GXAttrType GetVertexDescriptor(GXAttr attr) {return mVertexDescriptors[attr];};
  inline const VertexFormat& GetCurrentVertexFormat() {return mVertexFormats[mCurrentVertexFormat];};
  inline const VertexArray& GetVertexArray(GXAttr attr) {return mVertexArrays[attr];};
  inline const VertexFormat& GetVertexFormat(GXVtxFmt formatIdx) {return mVertexFormats[formatIdx];};
  const std::array<float, 16>& GetPositionMatrix() const;
  const std::array<float, 16>& GetProjectionMatrix() const;

  void Reset();
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

 private:
  static std::array<float, 16> IdentityMatrix();
  static float WordToFloat(u32 word);
  void RefreshPositionMatrices(u32 firstAddress, u32 endAddress);
  void RefreshProjectionMatrix(u32 firstAddress, u32 endAddress);

  GXVtxFmt mCurrentVertexFormat = GX_VTXFMT0;
  GXPrimitive mCurrentPrimitive = GX_TRIANGLES;
  size_t mCurrentPositionMatrix = 0;
  std::array<GXAttrType, GX_VA_MAX_ATTR> mVertexDescriptors = {};
  std::array<VertexFormat, GX_MAX_VTXFMT> mVertexFormats = {};
  std::array<VertexArray, GX_VA_MAX_ATTR> mVertexArrays = {};
  std::array<u32, 0x1100> mXfMemory = {};
  std::array<std::array<float, 16>, 10> mPositionMatrices = {};
  std::array<bool, 10> mPositionMatrixValid = {};
  std::array<float, 16> mProjectionMatrix = {};
  bool mProjectionMatrixValid = false;
};

void InitGlobalState();
GlobalState& GetGlobalState();

}


#endif
