#ifndef LIBPORPOISE_SIM_GX_STATE_HPP
#define LIBPORPOISE_SIM_GX_STATE_HPP

#include <array>
#include <vector>

#include <dolphin/types.h>
#include <dolphin/gx/GXAttr.h>

namespace SIM::GX {

struct VertexAttributes {
    GXCompCnt mComponents;
    GXCompType mDataType;
};

struct VertexFormat {
    std::array<VertexAttributes, GX_VA_MAX_ATTR> mAttributes;
};

struct VertexArray {
    void * mArrayPtr;
    int mStride;
};

class GlobalState {
 public:
  GlobalState();
  ~GlobalState() = default;

  static GlobalState& GetInstance();

  size_t GetDescriptorSize(GXAttrType descriptorType, GXCompType dataType, bool isColorType = false);
  size_t GetNumBytesPerVertex();
  size_t GetNumPositionComponents(GXCompCnt compType);

  GXAttrType GetVertexDescriptor(GXAttr attr);
  const VertexFormat& GetCurrentVertexFormat();
  const VertexArray& GetVertexArray(GXAttr attr);
  const VertexFormat& GetVertexFormat(GXVtxFmt formatIdx);

  void SetCurrentPrimitive(GXPrimitive primitive);
  void SetCurrentVertexFormat(GXVtxFmt format);
  void SetVertexArray(GXAttr attr, VertexArray array);
  void SetVertexDescriptor(GXAttr attr, GXAttrType descType);
  void SetVertexFormatComponents(GXVtxFmt formatIndex, GXAttr attrIndex, GXCompCnt component);
  void SetVertexFormatDataType(GXVtxFmt formatIndex, GXAttr attrIndex, GXCompType dataType);

 private:
  GXVtxFmt mCurrentVertexFormat;
  GXPrimitive mCurrentPrimitive;
  std::array<GXAttrType, GX_VA_MAX_ATTR> mVertexDescriptors = {};
  std::array<VertexFormat, GX_MAX_VTXFMT> mVertexFormats = {};
  std::array<VertexArray, GX_VA_MAX_ATTR> mVertexArrays = {};
};

void InitGlobalState();
GlobalState& GetGlobalState();

}


#endif