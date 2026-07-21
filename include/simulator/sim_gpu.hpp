#ifndef LIBPORPOISE_SIM_GPU_HPP
#define LIBPORPOISE_SIM_GPU_HPP

#include <dolphin/types.h>

#include <array>
#include <vector>

#include <dolphin/gx/GXAttr.h>

namespace SIM {

struct Vertex {
  float x;
  float y;
  float z;
  float w;
  float r;
  float g;
  float b;
  float a;
};

class GPU {
 enum class Opcode
 {
   NoOp = 0x00,
 
   LoadBpReg = 0x61,
   LoadCpReg = 0x08,
   LoadXfReg = 0x10,
   LoadXfIndexA = 0x20,
   LoadXfIndexB = 0x28,
   LoadXfIndexC = 0x30,
   LoadXfIndexD = 0x38,
 
   CallDisplayList = 0x40,
   GX_CMD_UNKNOWN_METRICS = 0x44, //TODO: not sure what this is, it came from dolphin emu OpcodeDecoding.h
   InvalidateVertexCache = 0x48,
 
   BeginQuads = 0x80,
   BeginQuadStrip = 0x88,
   BeginTriangles = 0x90,
   BeginTriangleStrip = 0x98,
   BeginTriangleFan = 0xa0,
   BeginLines = 0xa8,
   BeginLineStrip = 0xb0,
   BeginPoints = 0xb8,
 };

 enum class State
 {
   ReadOpcode,
   ReadArguments,
   ReadGeometry,
   ReadXfRegData,
 };

 enum class VertexAttributeType
 {
  None = 0,
  Direct = 1,
  Index8 = 2,
  Index16 = 3,
 };

 public:
  GPU();
  ~GPU() = default;
  void ProcessFifoData(u8 * data, size_t len);
  template <typename DataType>
  void AddFifoData(DataType data);
  void SetVertexArray(GXAttr attr, void * ptr, int stride);

 private:
  int GetOpcodeArgSize(Opcode code);
  int GetNumBytesPerVertex();
  void ProcessOpcode();
  void ProcessGeometry();
  void ProcessCpReg(u8 regAddr, u32 value);
  static inline u32 GetRegValue(u32 reg, u32 size, u32 shift) { return reg >> shift & (1u << size) - 1; };

  State mCurrentState;
  Opcode mLastOpcode;
  int mRemainingArgBytes;
  int mRemainingGeometryBytes;
  int mRemainingXfRegData;
  u32 mXfRegAddr;
  std::vector<u8> mArgsVec;
  std::vector<u8> mGeometryVec;
  std::vector<u8> mXfRegDataVec;
  GXCompCnt mPositionComponent = GX_COMPCNT_NULL;
  GXCompCnt mNormalComponent = GX_COMPCNT_NULL;
  GXCompCnt mColorComponent = GX_COMPCNT_NULL;
  GXCompCnt mTexCoordComponent = GX_COMPCNT_NULL;
  std::array<VertexAttributeType, GX_VA_MAX_ATTR> mVertexAttributes = {};
                    /* ptr    stride */
  std::array<std::pair<void*, int>, GX_VA_MAX_ATTR> mVertexArrays = {};
  GXPrimitive mPrimitiveType;
  std::vector<SIM::Vertex> mVertsOut;
};



}

#endif