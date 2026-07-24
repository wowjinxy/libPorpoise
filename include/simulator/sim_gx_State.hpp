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
  size_t GetNumPositionComponents(GXCompCnt compType);

  GXPrimitive GetCurrentPrimitive() const;
  GXAttrType GetVertexDescriptor(GXAttr attr);
  const VertexFormat& GetCurrentVertexFormat();
  const VertexArray& GetVertexArray(GXAttr attr);
  const VertexFormat& GetVertexFormat(GXVtxFmt formatIdx);
  const std::array<float, 16>& GetPositionMatrix() const;
  const std::array<float, 16>& GetProjectionMatrix() const;

  void Reset();
  void SetCurrentPrimitive(GXPrimitive primitive);
  void SetCurrentPositionMatrix(u32 matrixId);
  void SetCurrentVertexFormat(GXVtxFmt format);
  void SetVertexArray(GXAttr attr, VertexArray array);
  void SetVertexDescriptor(GXAttr attr, GXAttrType descType);
  void SetVertexFormatComponents(GXVtxFmt formatIndex, GXAttr attrIndex, GXCompCnt component);
  void SetVertexFormatDataType(GXVtxFmt formatIndex, GXAttr attrIndex, GXCompType dataType);
  void SetVertexFormatFraction(GXVtxFmt formatIndex, GXAttr attrIndex, u8 fraction);
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
