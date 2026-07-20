#include <dolphin.h>
#include <simulator/sim_gpu.h>
#include <simulator/sim_gpu.hpp>
#include <simulator/sim.h>
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
    }
}

void GPU::ProcessFifoDataU16(u16 data) {
    if(mCurrentState == GPU::State::ReadArguments && mRemainingArgBytes >= sizeof(u16)) {
        u16 * argsPointer16 = (u16*)mArgsBufferPointer;
        *argsPointer16 = data;
        mArgsBufferPointer += sizeof(u16);
        mRemainingArgBytes -= sizeof(u16);
        if(mRemainingArgBytes == 0) {
            ProcessOpcode();
        }
    }
}

void GPU::ProcessFifoDataU32(u32 data) {
    if(mCurrentState == GPU::State::ReadArguments && mRemainingArgBytes >= sizeof(u32)) {
        u32 * argsPointer32 = (u32*)mArgsBufferPointer;
        *argsPointer32 = data;
        mArgsBufferPointer += sizeof(u32);
        mRemainingArgBytes -= sizeof(u32);
        if(mRemainingArgBytes == 0) {
            ProcessOpcode();
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
                OSReport("LoadBpReg 0x%x\n", bpRegArgs);
            }
            break;
        case Opcode::LoadXfReg:
            {
                u32 xfRegArgs = *(u32*)mArgsBufferPointer;
                OSReport("LoadXfReg 0x%x\n", xfRegArgs);
            }
            break;
        case Opcode::LoadCpReg:
            OSReport("LoadCpReg\n");
            break;
        case Opcode::BeginTriangles:
            OSReport("Begin Triangles\n");
            break;
        case Opcode::BeginTriangleStrip:
            OSReport("Begin Triangle strip\n");
            break;
        case Opcode::BeginQuadStrip:
            OSReport("Begin Quad Strip\n");
            break;
        case Opcode::BeginTriangleFan:
            OSReport("Begin Triangle fan\n");
            break;
        case Opcode::BeginLines:
            OSReport("Begin Lines\n");
            break;
        case Opcode::BeginLineStrip:
            OSReport("Begin Line Strip\n");
            break;
        case Opcode::BeginPoints:
            OSReport("Begin Points\n");
            break;
        case Opcode::BeginQuads:
            OSReport("Begin Quads\n");
            break;
        case Opcode::InvalidateVertexCache:
            OSReport("InvalidateVertexCache\n");
            break;
        default:
            OSReport("Unknown opcode\n");
            break;
    }

    mCurrentState = State::ReadOpcode;

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
    sGPU->ProcessFifoDataU16(data);
}

void SIM_GPU_FifoSendU32(u32 data) {
    sGPU->ProcessFifoDataU32(data);
}