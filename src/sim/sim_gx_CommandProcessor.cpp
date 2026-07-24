#include "dolphin/gx/GXEnum.h"
#include <dolphin.h>
#include <simulator/glad/glad.h>
#include <simulator/sim_gx_CommandProcessor.h>
#include <simulator/sim_gx_CommandProcessor.hpp>
#include <simulator/sim_gx_State.hpp>
#include <simulator/sim.h>
#include <cstdlib>
#include <string.h>


static GLuint gpuVertexArray;
static GLuint gpuVertexBuffer;

namespace SIM::GX {
CommandProcessor::CommandProcessor() : mGeometryProcessor(GeometryProcessor()),
             mCurrentState(CommandProcessor::State::ReadOpcode),
             mLastOpcode(CommandProcessor::Opcode::NoOp),
             mRemainingArgBytes(0){
}

void CommandProcessor::ProcessFifoData(u8 * data, size_t len) {
    while(len > 0) {
        if(mCurrentState == CommandProcessor::State::ReadOpcode) {
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
        } else if(mCurrentState == CommandProcessor::State::ReadArguments) {
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
        } else if(mCurrentState == CommandProcessor::State::ReadGeometry) {
            if(mRemainingGeometryBytes <= 0 ) {
                OSReport("GPU: Geometry error.\n");
            }

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
        } else if(mCurrentState == State::ReadXfRegData) {
            if(mRemainingXfRegData <= 0 ) {
                OSReport("GPU: XfRegData error.\n");
            }

            size_t xfRegDataLen = std::min<size_t>(mRemainingXfRegData, len);
            for(auto i = 0; i < xfRegDataLen; i++) {
                mXfRegDataVec.push_back(*data);
                data++;
                len--;
                mRemainingXfRegData--;
            }

            if(mRemainingXfRegData <= 0 ) {
                //todo: process the xf reg data
                mCurrentState = State::ReadOpcode;
                mXfRegDataVec.clear();
            }
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

    mRemainingGeometryBytes = numVerts * GetGlobalState().GetNumBytesPerVertex();
    mTotalGeometryBytes = mRemainingGeometryBytes;
    mCurrentState = State::ReadGeometry;
}

void CommandProcessor::ProcessOpcode() {
    switch(mLastOpcode) {
        case Opcode::NoOp:
            mCurrentState = State::ReadOpcode;
            break;
        case Opcode::LoadBpReg:
            {
                u32 bpRegArgs = *(u32*)mArgsVec.data();
                OSReport("LoadBpReg 0x%x\n", bpRegArgs);
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
                mArgsVec.erase(mArgsVec.begin());
                u32 value = *(u32*)mArgsVec.data();
                OSReport("LoadCpReg 0x%x : %x\n", addr, value);
                ProcessCpReg(addr, value);
                mCurrentState = State::ReadOpcode;
            }
            break;
        case Opcode::BeginTriangles:
            {
              u16 numVerts = *(u16*)mArgsVec.data();
              HandleBeginPrimitive(GX_TRIANGLES, numVerts);
              OSReport("Begin Triangles %d\n", numVerts);
            }
            break;
        case Opcode::BeginTriangleStrip:
            {
              u16 numVerts = *(u16*)mArgsVec.data();
              HandleBeginPrimitive(GX_TRIANGLESTRIP, numVerts);
              OSReport("Begin Triangle strip %d\n", numVerts);
            }
            break;
        case Opcode::BeginQuadStrip:
            {
              u16 numVerts = *(u16*)mArgsVec.data();
              //NOTE: GX_QUADSTRIP does not exist??
              HandleBeginPrimitive(GX_QUADS, numVerts);
              OSReport("Begin Quad strip %d\n", numVerts);
            }
            break;
        case Opcode::BeginTriangleFan:
            {
              u16 numVerts = *(u16*)mArgsVec.data();
              HandleBeginPrimitive(GX_TRIANGLEFAN, numVerts);
              OSReport("Begin Triangle Fan %d\n", numVerts);
            }
            break;
        case Opcode::BeginLines:
            {
              u16 numVerts = *(u16*)mArgsVec.data();
              HandleBeginPrimitive(GX_LINES, numVerts);
              OSReport("Begin Lines %d\n", numVerts);
            }
            break;
        case Opcode::BeginLineStrip:
            {
              u16 numVerts = *(u16*)mArgsVec.data();
              HandleBeginPrimitive(GX_LINESTRIP, numVerts);
              OSReport("Begin Line Strip %d\n", numVerts);
            }
            break;
        case Opcode::BeginPoints:
            {
              u16 numVerts = *(u16*)mArgsVec.data();
              HandleBeginPrimitive(GX_POINTS, numVerts);
              OSReport("Begin Points %d\n", numVerts);
            }
            break;
        case Opcode::BeginQuads:
            {
              u16 numVerts = *(u16*)mArgsVec.data();
              HandleBeginPrimitive(GX_QUADS, numVerts);
              OSReport("Begin Quads %d\n", numVerts);
            }
            break;
        case Opcode::InvalidateVertexCache:
            OSReport("InvalidateVertexCache\n");
            mCurrentState = State::ReadOpcode;
            break;
        default:
            OSReport("Unknown opcode 0x%x\n", mLastOpcode);
            mCurrentState = State::ReadOpcode;
            break;
    }
    mArgsVec.clear();
}

void CommandProcessor::ProcessCpReg(u8 regAddr, u32 value) {
    switch(regAddr) {

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
                    gxState.SetVertexFormatComponents(formatIndex, GX_VA_POS, static_cast<GXCompCnt>(GetRegValue(value, 1, 0)));
                    gxState.SetVertexFormatDataType(formatIndex, GX_VA_POS, static_cast<GXCompType>(GetRegValue(value, 3, 1)));
                    gxState.SetVertexFormatFraction(formatIndex, GX_VA_POS, GetRegValue(value, 5, 4));
                    //TODO: Normal component

                    gxState.SetVertexFormatComponents(formatIndex, GX_VA_CLR0, static_cast<GXCompCnt>(GetRegValue(value, 1, 13)));
                    gxState.SetVertexFormatDataType(formatIndex, GX_VA_CLR1, static_cast<GXCompType>(GetRegValue(value, 3, 14)));
                } else if(regAddr >= 0x80 && regAddr <= 0x87) {

                } else if(regAddr >= 0x90 && regAddr <= 0x97) {

                }
            }
            break;
    }
}

}

static SIM::GX::CommandProcessor * sCommandProcessor;

void SIM_GX_CommandProcessor_Init() {
    sCommandProcessor = new SIM::GX::CommandProcessor();

    // TODO: the gl stuff might move to another file
    //glGenVertexArrays(1, &gpuVertexArray);
    //glBindVertexArray(gpuVertexArray);
    //glGenBuffers(1, &gpuVertexBuffer);

    //glBindVertexArray(gpuVertexArray);
    //glBindBuffer(GL_ARRAY_BUFFER, gpuVertexBuffer);
    //glEnableVertexAttribArray(0);
    //glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SIM::Vertex), (void*)offsetof(SIM::Vertex, x));
}

// C APIs for GX CommandProcessor
void SIM_GX_CommandProcessor_SendU8(u8 data) {
    sCommandProcessor->AddFifoData<u8>(data);
}

void SIM_GX_CommandProcessor_SendU16(u16 data) {
    sCommandProcessor->AddFifoData<u16>(data);
}

void SIM_GX_CommandProcessor_SendS16(s16 data) {
    sCommandProcessor->AddFifoData<s16>(data);
}

void SIM_GX_CommandProcessor_SendU32(u32 data) {
    sCommandProcessor->AddFifoData<u32>(data);
}

void SIM_GX_CommandProcessor_SendF32(f32 data) {
    sCommandProcessor->AddFifoData<f32>(data);
}

void SIM_GX_CommandProcessor_SendU64(u64 data) {
    sCommandProcessor->AddFifoData<u64>(data);
}

void SIM_GX_CommandProcessor_SetVertexArray(GXAttr attr, void * ptr, int stride) {
    SIM::GX::VertexArray vtxArray;
    vtxArray.mArrayPtr = ptr;
    vtxArray.mStride = stride;
    SIM::GX::GetGlobalState().SetVertexArray(attr, vtxArray);
}