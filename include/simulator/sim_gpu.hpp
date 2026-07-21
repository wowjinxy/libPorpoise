#ifndef LIBPORPOISE_SIM_GPU_HPP
#define LIBPORPOISE_SIM_GPU_HPP

#include <dolphin/types.h>
#include <vector>

namespace SIM {

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

 public:
  GPU();
  ~GPU() = default;
  void ProcessFifoData(u8 * data, size_t len);
  template <typename DataType>
  void AddFifoData(DataType data);

 private:
  int GetOpcodeArgSize(Opcode code);
  void ProcessOpcode();

  State mCurrentState;
  Opcode mLastOpcode;
  int mRemainingArgBytes;
  int mRemainingGeometryBytes;
  int mRemainingXfRegData;
  u32 mXfRegAddr;
  std::vector<u8> mArgsVec;
  std::vector<u8> mGeometryVec;
  std::vector<u8> mXfRegDataVec;
};



}

#endif