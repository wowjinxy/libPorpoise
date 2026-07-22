#ifndef LIBPORPOISE_SIM_GX_STATE_HPP
#define LIBPORPOISE_SIM_GX_STATE_HPP

#include <array>
#include <vector>

#include <dolphin/types.h>
#include <dolphin/gx/GXAttr.h>

//NOTES: this should probably move to src/gx and be kept internal to GX


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

  size_t GetNumBytesPerVertex();

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