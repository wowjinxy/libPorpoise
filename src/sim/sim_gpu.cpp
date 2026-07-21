#include <dolphin.h>
#include <simulator/sim_gpu.h>
#include <simulator/sim_gpu.hpp>
#include <simulator/sim.h>
#include <cstdlib>
#include <string.h>


namespace SIM {
GPU::GPU() : mCurrentState(GPU::State::ReadOpcode),
             mLastOpcode(GPU::Opcode::NoOp),
             mRemainingArgBytes(0){
}

void GPU::ProcessFifoData(u8 * data, size_t len) {
    while(len > 0) {
        if(mCurrentState == GPU::State::ReadOpcode) {
            GPU::Opcode code = static_cast<GPU::Opcode>(*data);
            data++;
            len--;
            int numArgs = GetOpcodeArgSize(code);
            mLastOpcode = code;
            if(numArgs <= 0) {
                ProcessOpcode();
            } else {
                mCurrentState = GPU::State::ReadArguments;
                mRemainingArgBytes = numArgs;
            }
        } else if(mCurrentState == GPU::State::ReadArguments) {
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
        } else if(mCurrentState == GPU::State::ReadGeometry) {
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
                mGeometryVec.clear();
                mCurrentState = GPU::State::ReadOpcode;
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
void GPU::AddFifoData(DataType data) {
    u8 * dataPtr = (u8*)&data;
    ProcessFifoData(dataPtr, sizeof(DataType));
}


int GPU::GetOpcodeArgSize(Opcode code) {
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

void GPU::ProcessOpcode() {
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
                mCurrentState = State::ReadOpcode;
            }
            break;
        case Opcode::BeginTriangles:
            //OSReport("Begin Triangles\n");
            mCurrentState = State::ReadOpcode;
            break;
        case Opcode::BeginTriangleStrip:
            //OSReport("Begin Triangle strip\n");
            mCurrentState = State::ReadOpcode;
            break;
        case Opcode::BeginQuadStrip:
            OSReport("Begin Quad Strip\n");
            mCurrentState = State::ReadOpcode;
            break;
        case Opcode::BeginTriangleFan:
            //OSReport("Begin Triangle fan\n");
            mCurrentState = State::ReadOpcode;
            break;
        case Opcode::BeginLines:
            //OSReport("Begin Lines\n");
            mCurrentState = State::ReadOpcode;
            break;
        case Opcode::BeginLineStrip:
            //OSReport("Begin Line Strip\n");
            mCurrentState = State::ReadOpcode;
            break;
        case Opcode::BeginPoints:
            //OSReport("Begin Points\n");
            mCurrentState = State::ReadOpcode;
            break;
        case Opcode::BeginQuads:
            {
              u16 quadsArgs = *(u16*)mArgsVec.data();
              OSReport("Begin Quads %d\n", quadsArgs);
              //mCurrentState = State::ReadGeometry;

              //mTotalGeometryBytes = mRemainingGeometryBytes = quadsArgs;
              //mGeometryBuf = (u8*)malloc(mTotalGeometryBytes);
              //mGeometryBufPointer = mGeometryBuf;
              //TODO: Fix the geometry stuff
              mCurrentState = State::ReadOpcode;
            }
            break;
        case Opcode::InvalidateVertexCache:
            OSReport("InvalidateVertexCache\n");
            mCurrentState = State::ReadOpcode;
            break;
        default:
            OSReport("Unknown opcode\n");
            mCurrentState = State::ReadOpcode;
            break;
    }
    mArgsVec.clear();
}
}

static SIM::GPU * sGPU;

void SIM_GPU_Init() {
    sGPU = new SIM::GPU();
}

// C APIs for GPU FIFO
void SIM_GPU_FifoSendU8(u8 data) {
    sGPU->AddFifoData<u8>(data);
}

void SIM_GPU_FifoSendU16(u16 data) {
    sGPU->AddFifoData<u16>(data);
}

void SIM_GPU_FifoSendS16(s16 data) {
    sGPU->AddFifoData<s16>(data);
}

void SIM_GPU_FifoSendU32(u32 data) {
    sGPU->AddFifoData<u32>(data);
}

void SIM_GPU_FifoSendF32(f32 data) {
    sGPU->AddFifoData<f32>(data);
}

void SIM_GPU_FifoSendU64(u64 data) {
    sGPU->AddFifoData<u64>(data);
}