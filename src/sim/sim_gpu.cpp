#include <dolphin.h>
#include <simulator/glad/glad.h>
#include <simulator/sim_gpu.h>
#include <simulator/sim_gpu.hpp>
#include <simulator/sim.h>
#include <cstdlib>
#include <string.h>


static GLuint gpuVertexArray;
static GLuint gpuVertexBuffer;

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
                ProcessGeometry();
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


void GPU::SetVertexArray(GXAttr attr, void * ptr, int stride) {
    mVertexArrays[attr].first = ptr;
    mVertexArrays[attr].second = stride;
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

int GPU::GetNumBytesPerVertex() {
    int totalBytes = 0;
    for(auto attribute : mVertexAttributes) {
        if(attribute != VertexAttributeType::None) {
            if(attribute == VertexAttributeType::Direct) {
                OSReport("Direct verts currently not implemented.\n");
            } else {
                totalBytes += (attribute == VertexAttributeType::Index16) ? 2
                                                                          : 1;
            }
        }
    }
    return totalBytes;
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
                ProcessCpReg(addr, value);
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
              u16 numVerts = *(u16*)mArgsVec.data();
              mPrimitiveType = GX_QUADS;
              OSReport("Begin Quads %d\n", numVerts);

              mRemainingGeometryBytes = numVerts * GetNumBytesPerVertex();
              mCurrentState = State::ReadGeometry;
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

void GPU::ProcessGeometry() {
    // TODO: Currently, this assumes all verts are s16 (vtx formats have not been implemented yet)
    // This also assumes the verts are indexed not direct
    // this makes a lot of assumptions currently...

    int numVerts = mGeometryVec.size() / GetNumBytesPerVertex();

    for(int i=0; i < numVerts; i++) {
        Vertex vtx;
        int arrayIdx = mGeometryVec[2*i];
        int stride = mVertexArrays[GX_VA_POS].second;
        u16 * array = (u16*)mVertexArrays[GX_VA_POS].first;
        vtx.x = static_cast<float>(array[(arrayIdx * stride)]) / 32767.0f;
        vtx.y = static_cast<float>(array[(arrayIdx * stride) + 1]) / 32767.0f;
        vtx.z = static_cast<float>(array[(arrayIdx * stride) + 2]) / 32767.0f;
        vtx.a = 1.0f;
        mVertsOut.push_back(vtx);
    }

    glBindVertexArray(gpuVertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, gpuVertexBuffer);

    glBufferData(GL_ARRAY_BUFFER, mVertsOut.size() * sizeof(SIM::Vertex), mVertsOut.data(), GL_STATIC_DRAW);
    glDisable( GL_CULL_FACE );

    glDrawArrays(GL_TRIANGLES,0, mVertsOut.size());

    mVertsOut.clear();
}

void GPU::ProcessCpReg(u8 regAddr, u32 value) {
    switch(regAddr) {

        // VCD low
        case 0x50: {
            mVertexAttributes[GX_VA_PNMTXIDX] = static_cast<VertexAttributeType>(GetRegValue(value, 1, 0));
            mVertexAttributes[GX_VA_TEX0MTXIDX] = static_cast<VertexAttributeType>(GetRegValue(value, 1, 1));
            mVertexAttributes[GX_VA_TEX1MTXIDX] = static_cast<VertexAttributeType>(GetRegValue(value, 1, 2));
            mVertexAttributes[GX_VA_TEX2MTXIDX] = static_cast<VertexAttributeType>(GetRegValue(value, 1, 3));
            mVertexAttributes[GX_VA_TEX3MTXIDX] = static_cast<VertexAttributeType>(GetRegValue(value, 1, 4));
            mVertexAttributes[GX_VA_TEX4MTXIDX] = static_cast<VertexAttributeType>(GetRegValue(value, 1, 5));
            mVertexAttributes[GX_VA_TEX5MTXIDX] = static_cast<VertexAttributeType>(GetRegValue(value, 1, 6));
            mVertexAttributes[GX_VA_TEX6MTXIDX] = static_cast<VertexAttributeType>(GetRegValue(value, 1, 7));
            mVertexAttributes[GX_VA_TEX7MTXIDX] = static_cast<VertexAttributeType>(GetRegValue(value, 1, 8));
            mVertexAttributes[GX_VA_POS] = static_cast<VertexAttributeType>(GetRegValue(value, 2, 9));
            mVertexAttributes[GX_VA_NRM] = static_cast<VertexAttributeType>(GetRegValue(value, 2, 11));
            mVertexAttributes[GX_VA_CLR0] = static_cast<VertexAttributeType>(GetRegValue(value, 2, 13));
            mVertexAttributes[GX_VA_CLR1] = static_cast<VertexAttributeType>(GetRegValue(value, 2, 15));
        }
        break;

        // VCD high
        case 0x60: {
            mVertexAttributes[GX_VA_TEX0] = static_cast<VertexAttributeType>(GetRegValue(value, 2, 0));
            mVertexAttributes[GX_VA_TEX1] = static_cast<VertexAttributeType>(GetRegValue(value, 2, 2));
            mVertexAttributes[GX_VA_TEX2] = static_cast<VertexAttributeType>(GetRegValue(value, 2, 4));
            mVertexAttributes[GX_VA_TEX3] = static_cast<VertexAttributeType>(GetRegValue(value, 2, 6));
            mVertexAttributes[GX_VA_TEX4] = static_cast<VertexAttributeType>(GetRegValue(value, 2, 8));
            mVertexAttributes[GX_VA_TEX5] = static_cast<VertexAttributeType>(GetRegValue(value, 2, 10));
            mVertexAttributes[GX_VA_TEX6] = static_cast<VertexAttributeType>(GetRegValue(value, 2, 12));
            mVertexAttributes[GX_VA_TEX7] = static_cast<VertexAttributeType>(GetRegValue(value, 2, 14));
        }
        break;

        default:
            break;
    }
}

}

static SIM::GPU * sGPU;

void SIM_GPU_Init() {
    sGPU = new SIM::GPU();

    // TODO: the gl stuff might move to another file
    glGenVertexArrays(1, &gpuVertexArray);
    glBindVertexArray(gpuVertexArray);
    glGenBuffers(1, &gpuVertexBuffer);

    glBindVertexArray(gpuVertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, gpuVertexBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SIM::Vertex), (void*)offsetof(SIM::Vertex, x));
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

void SIM_GPU_SetVertexArray(GXAttr attr, void * ptr, int stride) {
    sGPU->SetVertexArray(attr, ptr, stride);
}