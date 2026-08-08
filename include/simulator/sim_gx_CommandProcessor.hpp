#ifndef LIBPORPOISE_SIM_GX_COMMANDPROCESSOR_HPP
#define LIBPORPOISE_SIM_GX_COMMANDPROCESSOR_HPP

#include "dolphin/gx/GXEnum.h"
#include <dolphin/types.h>

#include <array>
#include <vector>

#include <dolphin/gx/GXAttr.h>
#include <SDL2/SDL.h>

#include "simulator/sim_gx_Geometry.hpp"
#include "simulator/sim_MessageQueue.hpp"

namespace SIM::GX {

class CommandProcessor {
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
  CommandProcessor();
  ~CommandProcessor() = default;
  void ProcessFifoData(u8 * data, size_t len);
  template <typename DataType>
  void AddFifoData(DataType data);

 private:
  int GetOpcodeArgSize(Opcode code);
  void HandleBeginPrimitive(GXPrimitive primitive, size_t numVerts);
  void ProcessOpcode();
  void ProcessCpReg(u8 regAddr, u32 value);
  static inline u32 GetRegValue(u32 reg, u32 size, u32 shift) {
    return (reg >> shift) & ((1u << size) - 1u);
  }

  GeometryProcessor mGeometryProcessor;
  State mCurrentState;
  Opcode mLastOpcode;
  int mRemainingArgBytes;
  int mRemainingGeometryBytes = 0;
  int mTotalGeometryBytes = 0;
  int mRemainingXfRegData = 0;
  u32 mXfRegAddr = 0;
  std::vector<u8> mArgsVec;
  std::vector<u8> mGeometryVec;
  std::vector<u8> mXfRegDataVec;
  GXCompCnt mPositionComponent = GX_COMPCNT_NULL;
  GXCompCnt mNormalComponent = GX_COMPCNT_NULL;
  GXCompCnt mColorComponent = GX_COMPCNT_NULL;
  GXCompCnt mTexCoordComponent = GX_COMPCNT_NULL;
  GXVtxFmt mLastVertexFormatIdx = GX_VTXFMT0;
};



}

#endif
