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
    memset(mArgsBuffer, 0, sizeof(mArgsBuffer));
    mArgsBufferPointer = &mArgsBuffer[0];
}

void GPU::ProcessFifoDataU8(u8 data) {
    if(mCurrentState == GPU::State::ReadOpcode) {
        GPU::Opcode code = static_cast<GPU::Opcode>(data);
        int numArgs = GetOpcodeArgSize(code);
        mLastOpcode = code;
        if(numArgs == 0) {
            ProcessOpcode();
        } else {
            mCurrentState = GPU::State::ReadArguments;
            mRemainingArgBytes = numArgs;
        }
    } else if(mCurrentState == GPU::State::ReadArguments) {
        if(mRemainingArgBytes == 0 ) {
            OSReport("GPU: Arguments error.\n");
        }

        *mArgsBufferPointer = data;
        mArgsBufferPointer++;

        mRemainingArgBytes -= 1;
    } else if(mCurrentState == GPU::State::ReadGeometry) {
        if(mRemainingGeometryBytes <= 0 ) {
            OSReport("GPU: Geometry error.\n");
        }
        *mGeometryBufPointer = data;
        mGeometryBufPointer++;
        mRemainingGeometryBytes--;

        if(mRemainingGeometryBytes == 0) {
            mCurrentState = GPU::State::ReadOpcode;
            free(mGeometryBuf);
            mGeometryBufPointer = nullptr;
        }
    }
}

template <typename DataType>
void GPU::ProcessFifoData(DataType data) {
    if(mCurrentState == GPU::State::ReadArguments && mRemainingArgBytes >= sizeof(DataType)) {
        DataType * argsPointer = (DataType*)mArgsBufferPointer;
        *argsPointer = data;
        mArgsBufferPointer += sizeof(DataType);
        mRemainingArgBytes -= sizeof(DataType);
        if(mRemainingArgBytes == 0) {
            ProcessOpcode();
        }
    } else if(mCurrentState == GPU::State::ReadGeometry && mRemainingGeometryBytes >= sizeof(DataType)) {
        if(mRemainingGeometryBytes <= 0 ) {
            OSReport("GPU: Geometry error.\n");
        }
        *mGeometryBufPointer = data;
        mGeometryBufPointer+= sizeof(DataType);
        mRemainingGeometryBytes-= sizeof(DataType);

        if(mRemainingGeometryBytes == 0) {
            mCurrentState = GPU::State::ReadOpcode;
            free(mGeometryBuf);
            mGeometryBufPointer = nullptr;
        } else {
            OSReport("GPU: Arguments error. Current state %d. Remaining Arg Bytes %d.\n", mCurrentState, mRemainingArgBytes);
        }
    }
}


int GPU::GetOpcodeArgSize(Opcode code) {
    switch(code) {
        case Opcode::LoadCpReg:
            return 5;
        case Opcode::LoadBpReg:
        case Opcode::LoadXfReg:
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
        case Opcode::NoOp:
        default:
            return 0;
    }
}

void GPU::ProcessOpcode() {
    mArgsBufferPointer = &mArgsBuffer[0];
    switch(mLastOpcode) {
        case Opcode::LoadBpReg:
            {
                u32 bpRegArgs = *(u32*)mArgsBufferPointer;
                //OSReport("LoadBpReg 0x%x\n", bpRegArgs);
                mCurrentState = State::ReadOpcode;
            }
            break;
        case Opcode::LoadXfReg:
            {
                u32 xfRegArgs = *(u32*)mArgsBufferPointer;
                //OSReport("LoadXfReg 0x%x\n", xfRegArgs);
                mCurrentState = State::ReadOpcode;
            }
            break;
        case Opcode::LoadCpReg:
            //OSReport("LoadCpReg\n");
            mCurrentState = State::ReadOpcode;
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
              u16 quadsArgs = *(u16*)mArgsBufferPointer;
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
            //OSReport("InvalidateVertexCache\n");
            mCurrentState = State::ReadOpcode;
            break;
        default:
            OSReport("Unknown opcode\n");
            mCurrentState = State::ReadOpcode;
            break;
    }

    memset(mArgsBuffer, 0, sizeof(mArgsBuffer));
}
}

static SIM::GPU * sGPU;

void SIM_GPU_Init() {
    sGPU = new SIM::GPU();
}

// C APIs for GPU FIFO
void SIM_GPU_FifoSendU8(u8 data) {
    sGPU->ProcessFifoDataU8(data);
}

void SIM_GPU_FifoSendU16(u16 data) {
    sGPU->ProcessFifoData<u16>(data);
}

void SIM_GPU_FifoSendS16(s16 data) {
    sGPU->ProcessFifoData<s16>(data);
}

void SIM_GPU_FifoSendU32(u32 data) {
    sGPU->ProcessFifoData<u32>(data);
}

void SIM_GPU_FifoSendF32(f32 data) {
    sGPU->ProcessFifoData<f32>(data);
}

void SIM_GPU_FifoSendU64(u64 data) {
    sGPU->ProcessFifoData<u64>(data);
}