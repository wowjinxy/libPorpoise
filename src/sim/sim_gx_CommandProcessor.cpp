#include "dolphin/gx/GXEnum.h"
#include <algorithm>
#include <cstring>

#include <dolphin.h>
#include <simulator/sim_gx_CommandProcessor.hpp>
#include <simulator/sim_gx_State.hpp>
#include <simulator/sim.h>

namespace SIM::GX {
CommandProcessor::CommandProcessor() : mGeometryProcessor(GeometryProcessor()),
             mCurrentState(CommandProcessor::State::ReadOpcode),
             mLastOpcode(CommandProcessor::Opcode::NoOp),
             mRemainingArgBytes(0){
}

void CommandProcessor::ProcessFifoData(u8 * data, size_t len) {
    while(len > 0) {

        switch(mCurrentState) {
            case CommandProcessor::State::ReadOpcode:
            {
                u8 currentByte = *data;
                if(currentByte >= 0x80) {
                    //This is a beginPrimitive opcode, extract out the Vtx Format
                    mLastVertexFormatIdx = static_cast<GXVtxFmt>(currentByte & GX_VTXFMT7);
                    currentByte = currentByte & ~(GX_VTXFMT7);
                }
                CommandProcessor::Opcode code = static_cast<CommandProcessor::Opcode>(currentByte);
                data++;
                len--;
                int numArgs = GetOpcodeArgSize(code);
                mLastOpcode = code;
                if(numArgs <= 0) {
                    ProcessOpcode();
                } else {
                    mCurrentState = CommandProcessor::State::ReadArguments;
                    mRemainingArgBytes = numArgs;
                }
            } break;
            case CommandProcessor::State::ReadArguments:
            {
                size_t argsLen = std::min<size_t>(mRemainingArgBytes, len);
                for(auto i = 0; i < argsLen; i++) {
                    mArgsVec.push_back(*data);
                    data++;
                    len--;
                    mRemainingArgBytes--;
                }
                if(mRemainingArgBytes <= 0) {
                    ProcessOpcode();
                }
            } break;
            case CommandProcessor::State::ReadGeometry:
            {
                size_t geometryLen = std::min<size_t>(mRemainingGeometryBytes, len);
                for(auto i = 0; i < geometryLen; i++) {
                    mGeometryVec.push_back(*data);
                    data++;
                    len--;
                    mRemainingGeometryBytes--;
                }

                if(mRemainingGeometryBytes <= 0) {
                    mGeometryProcessor.ProcessByteStream(mGeometryVec);
                    mGeometryVec.clear();
                    mCurrentState = CommandProcessor::State::ReadOpcode;
                }
            } break;
            case CommandProcessor::State::ReadXfRegData:
            {
                size_t xfRegDataLen = std::min<size_t>(mRemainingXfRegData, len);
                for(auto i = 0; i < xfRegDataLen; i++) {
                    mXfRegDataVec.push_back(*data);
                    data++;
                    len--;
                    mRemainingXfRegData--;
                }

                if(mRemainingXfRegData <= 0 ) {
                    GetGlobalState().SetXfData(
                        mXfRegAddr,
                        mXfRegDataVec.data(),
                        mXfRegDataVec.size() / sizeof(u32));
                    mCurrentState = State::ReadOpcode;
                    mXfRegDataVec.clear();
                }
            } break;
            default:
                OSReport("SIM::GX: Invalid CommandProcessor State!\n");
        }
    }
}

template <typename DataType>
void CommandProcessor::AddFifoData(DataType data) {
    u8 * dataPtr = (u8*)&data;
    ProcessFifoData(dataPtr, sizeof(DataType));
}


int CommandProcessor::GetOpcodeArgSize(Opcode code) {
    switch(code) {
        case Opcode::LoadXfReg:
            return 4;
        case Opcode::LoadCpReg:
            return 5;
        case Opcode::LoadBpReg:
            return 4;
        case Opcode::BeginTriangles:
        case Opcode::BeginTriangleStrip:
        case Opcode::BeginQuads:
        case Opcode::BeginQuadStrip:
        case Opcode::BeginTriangleFan:
        case Opcode::BeginLines:
        case Opcode::BeginLineStrip:
        case Opcode::BeginPoints:
            return 2;
        case Opcode::InvalidateVertexCache:
        case Opcode::NoOp:
            return 0;
        default:
            OSReport("Unknown opcode\n");
            return 0;
    }
}

void CommandProcessor::HandleBeginPrimitive(GXPrimitive primitive, size_t numVerts) {
    auto& gxState = GetGlobalState();
    gxState.SetCurrentPrimitive(primitive);
    gxState.SetCurrentVertexFormat(mLastVertexFormatIdx);

    const size_t bytesPerVertex = gxState.GetNumBytesPerVertex();
    mRemainingGeometryBytes = static_cast<int>(numVerts * bytesPerVertex);
    mTotalGeometryBytes = mRemainingGeometryBytes;
    if (mRemainingGeometryBytes == 0) {
        mCurrentState = State::ReadOpcode;
    } else {
        mCurrentState = State::ReadGeometry;
        mGeometryVec.reserve(mRemainingGeometryBytes);
    }
}

void CommandProcessor::ProcessOpcode() {
    switch(mLastOpcode) {
        case Opcode::NoOp:
            mCurrentState = State::ReadOpcode;
            break;
        case Opcode::LoadBpReg:
            {
                auto& gxState = GetGlobalState();
                u32 value = *(u32*)mArgsVec.data();
                u32 regId = value >> 24;
                if (regId == 0xFE) {
                  // BP mask write: applies to the next BP register write only
                  gxState.SetBpRegCache(regId, value & 0x00FFFFFF);
                } else {
                    const u32 ssMask = gxState.GetBpRegCache(0xFE);
                    gxState.SetBpRegCache(0xFE, 0x00FFFFFF);
                    value = (regId << 24) | (((gxState.GetBpRegCache(regId) & ~ssMask) | (value & ssMask)) & 0x00FFFFFF);

                    gxState.SetBpRegCache((u8)regId, value);
                    ProcessBpReg(regId, value);
                }
            
                mCurrentState = State::ReadOpcode;
            }
            break;
        case Opcode::LoadXfReg:
            {
                u32 xfRegArgs = *(u32*)mArgsVec.data();
                u32 xfRegDataCount = ((xfRegArgs >> 16) & 0xFFFF) + 1;
                mXfRegAddr = (xfRegArgs & 0xFFFF);
                mRemainingXfRegData = xfRegDataCount * 4;
                
                mCurrentState = State::ReadXfRegData;
            }
            break;
        case Opcode::LoadCpReg:
            {
                u8 addr = mArgsVec[0];
                u32 value;
                std::memcpy(&value, mArgsVec.data() + 1, sizeof(value));
                ProcessCpReg(addr, value);
                mCurrentState = State::ReadOpcode;
            }
            break;
        case Opcode::BeginTriangles:
            {
              u16 numVerts;
              std::memcpy(&numVerts, mArgsVec.data(), sizeof(numVerts));
              HandleBeginPrimitive(GX_TRIANGLES, numVerts);
            }
            break;
        case Opcode::BeginTriangleStrip:
            {
              u16 numVerts;
              std::memcpy(&numVerts, mArgsVec.data(), sizeof(numVerts));
              HandleBeginPrimitive(GX_TRIANGLESTRIP, numVerts);
            }
            break;
        case Opcode::BeginQuadStrip:
            {
              u16 numVerts;
              std::memcpy(&numVerts, mArgsVec.data(), sizeof(numVerts));
              HandleBeginPrimitive(GX_QUADSTRIP, numVerts);
            }
            break;
        case Opcode::BeginTriangleFan:
            {
              u16 numVerts;
              std::memcpy(&numVerts, mArgsVec.data(), sizeof(numVerts));
              HandleBeginPrimitive(GX_TRIANGLEFAN, numVerts);
            }
            break;
        case Opcode::BeginLines:
            {
              u16 numVerts;
              std::memcpy(&numVerts, mArgsVec.data(), sizeof(numVerts));
              HandleBeginPrimitive(GX_LINES, numVerts);
            }
            break;
        case Opcode::BeginLineStrip:
            {
              u16 numVerts;
              std::memcpy(&numVerts, mArgsVec.data(), sizeof(numVerts));
              HandleBeginPrimitive(GX_LINESTRIP, numVerts);
            }
            break;
        case Opcode::BeginPoints:
            {
              u16 numVerts;
              std::memcpy(&numVerts, mArgsVec.data(), sizeof(numVerts));
              HandleBeginPrimitive(GX_POINTS, numVerts);
            }
            break;
        case Opcode::BeginQuads:
            {
              u16 numVerts;
              std::memcpy(&numVerts, mArgsVec.data(), sizeof(numVerts));
              HandleBeginPrimitive(GX_QUADS, numVerts);
            }
            break;
        case Opcode::InvalidateVertexCache:
            mCurrentState = State::ReadOpcode;
            break;
        default:
            OSReport("Unknown opcode 0x%x\n", mLastOpcode);
            mCurrentState = State::ReadOpcode;
            break;
    }
    mArgsVec.clear();
}

void CommandProcessor::ProcessBpReg(u8 regAddr, u32 value) {
    auto& gxState = GetGlobalState();
    switch(regAddr) {
        // GenMode
        case 0x00:
            {
                gxState.SetNumTexGens(GetRegValue(value, 4, 0));
                gxState.SetNumChannels(GetRegValue(value, 3, 4));
                gxState.SetNumTevStages(GetRegValue(value, 4, 10) + 1);
                GXCullMode hwCull = static_cast<GXCullMode>(GetRegValue(value, 2, 14));
                // BP encodes front/back opposite the GX representation
                switch (hwCull) {
                case GX_CULL_FRONT:
                  gxState.SetCullMode(GX_CULL_BACK);
                  break;
                case GX_CULL_BACK:
                  gxState.SetCullMode(GX_CULL_FRONT);
                  break;
                default:
                  gxState.SetCullMode(hwCull);
                  break;
                }
                //g_gxState.numIndStages = reg_get(value, 3, 16);
            } break;
        //0x10-0x1F: TEV indirect stage config
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1E:
        case 0x1F:
            {
                u8 stage = regAddr - 0x10;
                //if (stage >= GX_MAXTEVSTAGE) {
                //  return;
                //}
                //auto& s = g_gxState.tevStages[stage];
                //s.indTexStage = static_cast<GXIndTexStageID>(reg_get(value, 2, 0));
                //s.indTexFormat = static_cast<GXIndTexFormat>(reg_get(value, 2, 2));
                //s.indTexBiasSel = static_cast<GXIndTexBiasSel>(reg_get(value, 3, 4));
                //s.indTexAlphaSel = static_cast<GXIndTexAlphaSel>(reg_get(value, 2, 7));
                //s.indTexMtxId = static_cast<GXIndTexMtxID>(reg_get(value, 4, 9));
                //s.indTexWrapS = static_cast<GXIndTexWrap>(reg_get(value, 3, 13));
                //s.indTexWrapT = static_cast<GXIndTexWrap>(reg_get(value, 3, 16));
                //s.indTexUseOrigLOD = reg_get(value, 1, 19) != 0;
                //s.indTexAddPrev = reg_get(value, 1, 20) != 0;
            } break;
        //TEV Order (0x28-0x2F)
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2B:
        case 0x2C:
        case 0x2D:
        case 0x2F:
            {
                u8 idx = regAddr - 0x28;

                // Reverse mapping from hardware to GX
                static constexpr GXChannelID r2c[] = {GX_COLOR0A0, GX_COLOR1A1,   GX_COLOR0A0,    GX_COLOR1A1,
                                                      GX_COLOR0A0, GX_ALPHA_BUMP, GX_ALPHA_BUMPN, GX_COLOR_ZERO};
                
                for (u8 half = 0; half < 2; ++half) {
                  const u8 stage = idx * 2 + half;
                  if (stage >= GX_MAXTEVSTAGE) {
                    continue;
                  }
                  const u32 shift = half * 12;
                  auto& s = gxState.GetTevStageConfig(stage);
                  gxState.SetTevTexMap(stage, static_cast<GXTexMapID>(GetRegValue(value, 3, shift)));
                  s.mTexCoordId = static_cast<GXTexCoordID>(GetRegValue(value, 3, shift + 3));
                  if (!GetRegValue(value, 1, shift + 6)) {
                    gxState.SetTevTexMap(stage, GX_TEXMAP_NULL);
                  }
                  //u32 chanHw = GetRegValue(value, 3, shift + 7);
                  //s.channelId = (chanHw < 8) ? r2c[chanHw] : GX_COLOR_NULL;
                }
            } break;
        // TEV color combiner stages (0xC0, 0xC2, ... 0xDE)
        case 0xC0:
        case 0xC2:
        case 0xC4:
        case 0xC6:
        case 0xC8:
        case 0xCA:
        case 0xCC:
        case 0xCE:
        case 0xD0:
        case 0xD2:
        case 0xD4:
        case 0xD6:
        case 0xD8:
        case 0xDA:
        case 0xDC:
        case 0xDE:
            {
                u8 stage = (regAddr - 0xC0) / 2;
                if (stage >= GX_MAXTEVSTAGE) {
                  return;
                }
                auto& s = gxState.GetTevStageConfig(stage);
                s.mColorArgs[3] = static_cast<GXTevColorArg>(GetRegValue(value, 4, 0));
                s.mColorArgs[2] = static_cast<GXTevColorArg>(GetRegValue(value, 4, 4));
                s.mColorArgs[1] = static_cast<GXTevColorArg>(GetRegValue(value, 4, 8));
                s.mColorArgs[0] = static_cast<GXTevColorArg>(GetRegValue(value, 4, 12));
                //s.mClampMode = static_cast<GXTevClampMode>(GetRegValue(value, 1, 19) != 0);
                s.mOutReg = static_cast<GXTevRegID>(GetRegValue(value, 2, 22));
                if (GetRegValue(value, 2, 16) == 3) {
                  u32 hwOp = GetRegValue(value, 1, 18) | (GetRegValue(value, 2, 20) << 1);
                  s.mColorOperation = static_cast<GXTevOp>(hwOp + 8);
                  s.mBias = GX_TB_ZERO;
                  s.mScale = GX_CS_SCALE_1;
                } else {
                  s.mColorOperation = static_cast<GXTevOp>(GetRegValue(value, 1, 18));
                  s.mBias = static_cast<GXTevBias>(GetRegValue(value, 2, 16));
                  s.mScale = static_cast<GXTevScale>(GetRegValue(value, 2, 20));
                }
            } break;
        // TEV alpha combiner stages (0xC1, 0xC3, ... 0xDF)
        case 0xC1:
        case 0xC3:
        case 0xC5:
        case 0xC7:
        case 0xC9:
        case 0xCB:
        case 0xCD:
        case 0xCF:
        case 0xD1:
        case 0xD3:
        case 0xD5:
        case 0xD7:
        case 0xD9:
        case 0xDB:
        case 0xDD:
        case 0xDF:
            {
                u8 stage = (regAddr - 0xC1) / 2;
                if (stage >= GX_MAXTEVSTAGE) {
                  return;
                }
                auto& s = gxState.GetTevStageConfig(stage);
                //s.tevSwapRas = static_cast<GXTevSwapSel>(reg_get(value, 2, 0));
                //s.tevSwapTex = static_cast<GXTevSwapSel>(reg_get(value, 2, 2));
                s.mAlphaArgs[3] = static_cast<GXTevAlphaArg>(GetRegValue(value, 3, 4));
                s.mAlphaArgs[2] = static_cast<GXTevAlphaArg>(GetRegValue(value, 3, 7));
                s.mAlphaArgs[1] = static_cast<GXTevAlphaArg>(GetRegValue(value, 3, 10));
                s.mAlphaArgs[0] = static_cast<GXTevAlphaArg>(GetRegValue(value, 3, 13));
                //s.alphaOp.clamp = reg_get(value, 1, 19) != 0;
                //s.alphaOp.outReg = static_cast<GXTevRegID>(reg_get(value, 2, 22));
                if (GetRegValue(value, 2, 16) == 3) {
                  u32 hwOp = GetRegValue(value, 1, 18) | (GetRegValue(value, 2, 20) << 1);
                  s.mAlphaOperation = static_cast<GXTevOp>(hwOp + 8);
                  //s.alphaOp.bias = GX_TB_ZERO;
                  //s.alphaOp.scale = GX_CS_SCALE_1;
                } else {
                  //s.alphaOp.op = static_cast<GXTevOp>(GetRegValue(value, 1, 18));
                  //s.alphaOp.bias = static_cast<GXTevBias>(GetRegValue(value, 2, 16));
                  //s.alphaOp.scale = static_cast<GXTevScale>(GetRegValue(value, 2, 20));
                }
            } break;
        // Tev Regs (0xE0-0xE7)
        case 0xE0:
        case 0xE1:
        case 0xE2:
        case 0xE3:
        case 0xE4:
        case 0xE5:
        case 0xE6:
        case 0xE7:
            {
                u32 idx = (regAddr - 0xE0) / 2;
                bool isRA = (regAddr & 1) == 0;
                if (GetRegValue(value, 1, 23) != 0) {
                  // K color register (8-bit components)
                  if (idx < GX_MAX_KCOLOR) {
                    //auto& kc = g_gxState.kcolors[idx];
                    //if (isRA) {
                    //  kc[0] = static_cast<float>(reg_get(value, 8, 0)) / 255.f;  // R
                    //  kc[3] = static_cast<float>(reg_get(value, 8, 12)) / 255.f; // A
                    //} else {
                    //  kc[2] = static_cast<float>(reg_get(value, 8, 0)) / 255.f;  // B
                    //  kc[1] = static_cast<float>(reg_get(value, 8, 12)) / 255.f; // G
                    //}
                  }
                } else {
                  // TEV color register (11-bit signed components)
                  if (idx < 4) {
                    auto color = gxState.GetTevColor(idx);
                    if (isRA) {
                      color[0] = static_cast<float>((GetRegValue(value, 11, 0))) / 255.f;
                      color[3] = static_cast<float>((GetRegValue(value, 11, 12))) / 255.f;
                    } else {
                      color[2] = static_cast<float>((GetRegValue(value, 11, 0))) / 255.f;
                      color[1] = static_cast<float>((GetRegValue(value, 11, 12))) / 255.f;
                    }
                    gxState.SetTevColor(idx, color);
                  }
                }
            } break;
        default:
            break;
    }
}

void CommandProcessor::ProcessCpReg(u8 regAddr, u32 value) {
    switch(regAddr) {
        // Matrix index A. The position matrix index occupies bits 0..5.
        case 0x30:
            GetGlobalState().SetCurrentPositionMatrix(GetRegValue(value, 6, 0));
            break;

        // VCD low
        case 0x50: {
            auto& gxState = GetGlobalState();
            gxState.SetVertexDescriptor(GX_VA_PNMTXIDX, static_cast<GXAttrType>(GetRegValue(value, 1, 0)));
            gxState.SetVertexDescriptor(GX_VA_TEX0MTXIDX, static_cast<GXAttrType>(GetRegValue(value, 1, 1)));
            gxState.SetVertexDescriptor(GX_VA_TEX1MTXIDX, static_cast<GXAttrType>(GetRegValue(value, 1, 2)));
            gxState.SetVertexDescriptor(GX_VA_TEX2MTXIDX, static_cast<GXAttrType>(GetRegValue(value, 1, 3)));
            gxState.SetVertexDescriptor(GX_VA_TEX3MTXIDX, static_cast<GXAttrType>(GetRegValue(value, 1, 4)));
            gxState.SetVertexDescriptor(GX_VA_TEX4MTXIDX, static_cast<GXAttrType>(GetRegValue(value, 1, 5)));
            gxState.SetVertexDescriptor(GX_VA_TEX5MTXIDX, static_cast<GXAttrType>(GetRegValue(value, 1, 6)));
            gxState.SetVertexDescriptor(GX_VA_TEX6MTXIDX, static_cast<GXAttrType>(GetRegValue(value, 1, 7)));
            gxState.SetVertexDescriptor(GX_VA_TEX7MTXIDX, static_cast<GXAttrType>(GetRegValue(value, 1, 8)));
            gxState.SetVertexDescriptor(GX_VA_POS, static_cast<GXAttrType>(GetRegValue(value, 2, 9)));
            gxState.SetVertexDescriptor(GX_VA_NRM, static_cast<GXAttrType>(GetRegValue(value, 2, 11)));
            gxState.SetVertexDescriptor(GX_VA_CLR0, static_cast<GXAttrType>(GetRegValue(value, 2, 13)));
            gxState.SetVertexDescriptor(GX_VA_CLR1, static_cast<GXAttrType>(GetRegValue(value, 2, 15)));
        }
        break;

        // VCD high
        case 0x60: {
            //TODO: use global state instead
            auto& gxState = GetGlobalState();
            gxState.SetVertexDescriptor(GX_VA_TEX0, static_cast<GXAttrType>(GetRegValue(value, 2, 0)));
            gxState.SetVertexDescriptor(GX_VA_TEX1, static_cast<GXAttrType>(GetRegValue(value, 2, 2)));
            gxState.SetVertexDescriptor(GX_VA_TEX2, static_cast<GXAttrType>(GetRegValue(value, 2, 4)));
            gxState.SetVertexDescriptor(GX_VA_TEX3, static_cast<GXAttrType>(GetRegValue(value, 2, 6)));
            gxState.SetVertexDescriptor(GX_VA_TEX4, static_cast<GXAttrType>(GetRegValue(value, 2, 8)));
            gxState.SetVertexDescriptor(GX_VA_TEX5, static_cast<GXAttrType>(GetRegValue(value, 2, 10)));
            gxState.SetVertexDescriptor(GX_VA_TEX6, static_cast<GXAttrType>(GetRegValue(value, 2, 12)));
            gxState.SetVertexDescriptor(GX_VA_TEX7, static_cast<GXAttrType>(GetRegValue(value, 2, 14)));
        }
        break;

        default:
            {
                if(regAddr >= 0x70 && regAddr <= 0x77) {
                    GXVtxFmt formatIndex = (GXVtxFmt)(regAddr - 0x70);
                    auto& gxState = GetGlobalState();
                    //Position
                    gxState.SetVertexFormatComponents(formatIndex, GX_VA_POS, static_cast<GXCompCnt>(GetRegValue(value, 1, 0)));
                    gxState.SetVertexFormatDataType(formatIndex, GX_VA_POS, static_cast<GXCompType>(GetRegValue(value, 3, 1)));
                    gxState.SetVertexFormatFraction(formatIndex, GX_VA_POS, static_cast<u8>(GetRegValue(value, 5, 4)));
                    
                    //Normal
                    gxState.SetVertexFormatDataType(formatIndex, GX_VA_NRM, static_cast<GXCompType>(GetRegValue(value, 3, 10)));
                    

                    //Color0
                    gxState.SetVertexFormatComponents(formatIndex, GX_VA_CLR0, static_cast<GXCompCnt>(GetRegValue(value, 1, 13)));
                    gxState.SetVertexFormatDataType(formatIndex, GX_VA_CLR0, static_cast<GXCompType>(GetRegValue(value, 3, 14)));

                    //texCoord0
                    gxState.SetVertexFormatComponents(formatIndex, GX_VA_TEX0, static_cast<GXCompCnt>(GetRegValue(value, 1, 21)));
                    gxState.SetVertexFormatDataType(formatIndex, GX_VA_TEX0, static_cast<GXCompType>(GetRegValue(value, 3, 22)));
                    gxState.SetVertexFormatFraction(formatIndex, GX_VA_TEX0, static_cast<u8>(GetRegValue(value, 5, 25)));
                } else if(regAddr >= 0x80 && regAddr <= 0x87) {

                } else if(regAddr >= 0x90 && regAddr <= 0x97) {

                }
            }
            break;
    }
}

}
