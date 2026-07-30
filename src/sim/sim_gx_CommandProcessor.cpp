#include "dolphin/gx/GXEnum.h"
#include <algorithm>
#include <cstring>

#include <dolphin.h>
#include <simulator/sim_gx_CommandProcessor.h>
#include <simulator/sim_gx_CommandProcessor.hpp>
#include <simulator/sim_gx_State.hpp>
#include <simulator/sim.h>

extern "C" void __GXHostCompleteDrawSync(
    u16 token, GXBool signalCallback);
extern "C" void __GXHostRecordPrimitive(
    u32 primitive, u32 vertexCount, u32 textureStages);
extern "C" void __GXHostQueuePixelMetricReset(void);

namespace SIM::GX {
CommandProcessor::CommandProcessor() : mGeometryProcessor(GeometryProcessor()),
             mCurrentState(CommandProcessor::State::ReadOpcode),
             mLastOpcode(CommandProcessor::Opcode::NoOp),
             mRemainingArgBytes(0){
}

u16 CommandProcessor::ReadU16(const u8* data) const {
    if (mInputBigEndian) {
        return static_cast<u16>(
            (static_cast<u16>(data[0]) << 8) |
            static_cast<u16>(data[1]));
    }

    u16 value;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

u32 CommandProcessor::ReadU32(const u8* data) const {
    if (mInputBigEndian) {
        return
            (static_cast<u32>(data[0]) << 24) |
            (static_cast<u32>(data[1]) << 16) |
            (static_cast<u32>(data[2]) << 8) |
            static_cast<u32>(data[3]);
    }

    u32 value;
    std::memcpy(&value, data, sizeof(value));
    return value;
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
                mGeometryProcessor.ProcessByteStream(
                    mGeometryVec, mInputBigEndian);
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
                if (mInputBigEndian) {
                    std::vector<u8> nativeData(mXfRegDataVec.size());
                    for (size_t offset = 0;
                         offset < mXfRegDataVec.size();
                         offset += sizeof(u32)) {
                        const u32 word =
                            ReadU32(mXfRegDataVec.data() + offset);
                        std::memcpy(
                            nativeData.data() + offset,
                            &word,
                            sizeof(word));
                    }
                    GetGlobalState().SetXfData(
                        mXfRegAddr,
                        nativeData.data(),
                        nativeData.size() / sizeof(u32));
                } else {
                    GetGlobalState().SetXfData(
                        mXfRegAddr,
                        mXfRegDataVec.data(),
                        mXfRegDataVec.size() / sizeof(u32));
                }
                mCurrentState = State::ReadOpcode;
                mXfRegDataVec.clear();
            }
        }
    }
}

void CommandProcessor::ProcessDisplayList(const u8* data, size_t len) {
    if (data == nullptr || len == 0) {
        return;
    }

    const bool previousEndian = mInputBigEndian;
    mInputBigEndian = true;
    ProcessFifoData(const_cast<u8*>(data), len);
    mInputBigEndian = previousEndian;
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
        case Opcode::LoadXfIndexA:
        case Opcode::LoadXfIndexB:
        case Opcode::LoadXfIndexC:
        case Opcode::LoadXfIndexD:
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
        case Opcode::GX_CMD_UNKNOWN_METRICS:
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

    u32 textureStages = 0;
    for (size_t stage = 0; stage < gxState.GetNumTevStages(); ++stage) {
        if (gxState.GetTevStageState(stage).textureEnabled) {
            ++textureStages;
        }
    }
    __GXHostRecordPrimitive(
        static_cast<u32>(primitive),
        static_cast<u32>(numVerts),
        textureStages);

    const size_t bytesPerVertex = gxState.GetNumBytesPerVertex();
    mRemainingGeometryBytes = static_cast<int>(numVerts * bytesPerVertex);
    mTotalGeometryBytes = mRemainingGeometryBytes;
    if (mRemainingGeometryBytes == 0) {
        mCurrentState = State::ReadOpcode;
    } else {
        mCurrentState = State::ReadGeometry;
    }
}

void CommandProcessor::ProcessOpcode() {
    switch(mLastOpcode) {
        case Opcode::NoOp:
        case Opcode::GX_CMD_UNKNOWN_METRICS:
            mCurrentState = State::ReadOpcode;
            break;
        case Opcode::LoadBpReg:
            {
                ProcessBpReg(ReadU32(mArgsVec.data()));
                mCurrentState = State::ReadOpcode;
            }
            break;
        case Opcode::LoadXfReg:
            {
                u32 xfRegArgs = ReadU32(mArgsVec.data());
                u32 xfRegDataCount = ((xfRegArgs >> 16) & 0xFFFF) + 1;
                mXfRegAddr = (xfRegArgs & 0xFFFF);
                mRemainingXfRegData = xfRegDataCount * 4;
                
                mCurrentState = State::ReadXfRegData;
            }
            break;
        case Opcode::LoadCpReg:
            {
                u8 addr = mArgsVec[0];
                u32 value = ReadU32(mArgsVec.data() + 1);
                ProcessCpReg(addr, value);
                mCurrentState = State::ReadOpcode;
            }
            break;
        case Opcode::BeginTriangles:
            {
              u16 numVerts = ReadU16(mArgsVec.data());
              HandleBeginPrimitive(GX_TRIANGLES, numVerts);
            }
            break;
        case Opcode::LoadXfIndexA:
        case Opcode::LoadXfIndexB:
        case Opcode::LoadXfIndexC:
        case Opcode::LoadXfIndexD:
            {
                u32 command = ReadU32(mArgsVec.data());

                GXAttr arrayAttribute = GX_POS_MTX_ARRAY;
                if (mLastOpcode == Opcode::LoadXfIndexB) {
                    arrayAttribute = GX_NRM_MTX_ARRAY;
                } else if (mLastOpcode == Opcode::LoadXfIndexC) {
                    arrayAttribute = GX_TEX_MTX_ARRAY;
                } else if (mLastOpcode == Opcode::LoadXfIndexD) {
                    arrayAttribute = GX_LIGHT_ARRAY;
                }

                const u32 destination = command & 0x0fffu;
                const size_t wordCount =
                    static_cast<size_t>((command >> 12) & 0x0fu) + 1u;
                const size_t arrayIndex =
                    static_cast<size_t>(command >> 16);
                auto& gxState = GetGlobalState();
                const auto& array = gxState.GetVertexArray(arrayAttribute);
                if (array.mArrayPtr != nullptr && array.mStride > 0) {
                    const u8* source =
                        static_cast<const u8*>(array.mArrayPtr) +
                        arrayIndex * static_cast<size_t>(array.mStride);
                    gxState.SetXfData(destination, source, wordCount);
                }
                mCurrentState = State::ReadOpcode;
            }
            break;
        case Opcode::BeginTriangleStrip:
            {
              u16 numVerts = ReadU16(mArgsVec.data());
              HandleBeginPrimitive(GX_TRIANGLESTRIP, numVerts);
            }
            break;
        case Opcode::BeginQuadStrip:
            {
              u16 numVerts = ReadU16(mArgsVec.data());
              HandleBeginPrimitive(GX_QUADSTRIP, numVerts);
            }
            break;
        case Opcode::BeginTriangleFan:
            {
              u16 numVerts = ReadU16(mArgsVec.data());
              HandleBeginPrimitive(GX_TRIANGLEFAN, numVerts);
            }
            break;
        case Opcode::BeginLines:
            {
              u16 numVerts = ReadU16(mArgsVec.data());
              HandleBeginPrimitive(GX_LINES, numVerts);
            }
            break;
        case Opcode::BeginLineStrip:
            {
              u16 numVerts = ReadU16(mArgsVec.data());
              HandleBeginPrimitive(GX_LINESTRIP, numVerts);
            }
            break;
        case Opcode::BeginPoints:
            {
              u16 numVerts = ReadU16(mArgsVec.data());
              HandleBeginPrimitive(GX_POINTS, numVerts);
            }
            break;
        case Opcode::BeginQuads:
            {
              u16 numVerts = ReadU16(mArgsVec.data());
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

void CommandProcessor::ProcessBpReg(u32 value) {
    const u8 address = static_cast<u8>(value >> 24);
    if (address == 0x47u || address == 0x48u) {
        __GXHostCompleteDrawSync(
            static_cast<u16>(value),
            address == 0x48u ? GX_TRUE : GX_FALSE);
    } else if (address == 0x57u &&
               (value & 0x00ffffffu) == 0x00000aaau) {
        __GXHostQueuePixelMetricReset();
    }
    GetGlobalState().SetBpRegister(value);
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
                    gxState.SetVertexFormatComponents(formatIndex, GX_VA_POS, static_cast<GXCompCnt>(GetRegValue(value, 1, 0)));
                    gxState.SetVertexFormatDataType(formatIndex, GX_VA_POS, static_cast<GXCompType>(GetRegValue(value, 3, 1)));
                    gxState.SetVertexFormatFraction(formatIndex, GX_VA_POS, static_cast<u8>(GetRegValue(value, 5, 4)));

                    GXCompCnt normalComponents =
                        static_cast<GXCompCnt>(GetRegValue(value, 1, 9));
                    if (normalComponents == GX_NRM_NBT &&
                        GetRegValue(value, 1, 31) != 0) {
                        normalComponents = GX_NRM_NBT3;
                    }
                    gxState.SetVertexFormatComponents(
                        formatIndex, GX_VA_NRM, normalComponents);
                    gxState.SetVertexFormatDataType(
                        formatIndex,
                        GX_VA_NRM,
                        static_cast<GXCompType>(GetRegValue(value, 3, 10)));

                    gxState.SetVertexFormatComponents(formatIndex, GX_VA_CLR0, static_cast<GXCompCnt>(GetRegValue(value, 1, 13)));
                    gxState.SetVertexFormatDataType(formatIndex, GX_VA_CLR0, static_cast<GXCompType>(GetRegValue(value, 3, 14)));
                    gxState.SetVertexFormatComponents(formatIndex, GX_VA_CLR1, static_cast<GXCompCnt>(GetRegValue(value, 1, 17)));
                    gxState.SetVertexFormatDataType(formatIndex, GX_VA_CLR1, static_cast<GXCompType>(GetRegValue(value, 3, 18)));
                    gxState.SetVertexFormatComponents(formatIndex, GX_VA_TEX0, static_cast<GXCompCnt>(GetRegValue(value, 1, 21)));
                    gxState.SetVertexFormatDataType(formatIndex, GX_VA_TEX0, static_cast<GXCompType>(GetRegValue(value, 3, 22)));
                    gxState.SetVertexFormatFraction(formatIndex, GX_VA_TEX0, static_cast<u8>(GetRegValue(value, 5, 25)));
                } else if(regAddr >= 0x80 && regAddr <= 0x87) {
                    GXVtxFmt formatIndex = (GXVtxFmt)(regAddr - 0x80);
                    auto& gxState = GetGlobalState();
                    gxState.SetVertexFormatComponents(formatIndex, GX_VA_TEX1, static_cast<GXCompCnt>(GetRegValue(value, 1, 0)));
                    gxState.SetVertexFormatDataType(formatIndex, GX_VA_TEX1, static_cast<GXCompType>(GetRegValue(value, 3, 1)));
                    gxState.SetVertexFormatFraction(formatIndex, GX_VA_TEX1, static_cast<u8>(GetRegValue(value, 5, 4)));
                    gxState.SetVertexFormatComponents(formatIndex, GX_VA_TEX2, static_cast<GXCompCnt>(GetRegValue(value, 1, 9)));
                    gxState.SetVertexFormatDataType(formatIndex, GX_VA_TEX2, static_cast<GXCompType>(GetRegValue(value, 3, 10)));
                    gxState.SetVertexFormatFraction(formatIndex, GX_VA_TEX2, static_cast<u8>(GetRegValue(value, 5, 13)));
                    gxState.SetVertexFormatComponents(formatIndex, GX_VA_TEX3, static_cast<GXCompCnt>(GetRegValue(value, 1, 18)));
                    gxState.SetVertexFormatDataType(formatIndex, GX_VA_TEX3, static_cast<GXCompType>(GetRegValue(value, 3, 19)));
                    gxState.SetVertexFormatFraction(formatIndex, GX_VA_TEX3, static_cast<u8>(GetRegValue(value, 5, 22)));
                    gxState.SetVertexFormatComponents(formatIndex, GX_VA_TEX4, static_cast<GXCompCnt>(GetRegValue(value, 1, 27)));
                    gxState.SetVertexFormatDataType(formatIndex, GX_VA_TEX4, static_cast<GXCompType>(GetRegValue(value, 3, 28)));
                } else if(regAddr >= 0x90 && regAddr <= 0x97) {
                    GXVtxFmt formatIndex = (GXVtxFmt)(regAddr - 0x90);
                    auto& gxState = GetGlobalState();
                    gxState.SetVertexFormatFraction(formatIndex, GX_VA_TEX4, static_cast<u8>(GetRegValue(value, 5, 0)));
                    gxState.SetVertexFormatComponents(formatIndex, GX_VA_TEX5, static_cast<GXCompCnt>(GetRegValue(value, 1, 5)));
                    gxState.SetVertexFormatDataType(formatIndex, GX_VA_TEX5, static_cast<GXCompType>(GetRegValue(value, 3, 6)));
                    gxState.SetVertexFormatFraction(formatIndex, GX_VA_TEX5, static_cast<u8>(GetRegValue(value, 5, 9)));
                    gxState.SetVertexFormatComponents(formatIndex, GX_VA_TEX6, static_cast<GXCompCnt>(GetRegValue(value, 1, 14)));
                    gxState.SetVertexFormatDataType(formatIndex, GX_VA_TEX6, static_cast<GXCompType>(GetRegValue(value, 3, 15)));
                    gxState.SetVertexFormatFraction(formatIndex, GX_VA_TEX6, static_cast<u8>(GetRegValue(value, 5, 18)));
                    gxState.SetVertexFormatComponents(formatIndex, GX_VA_TEX7, static_cast<GXCompCnt>(GetRegValue(value, 1, 23)));
                    gxState.SetVertexFormatDataType(formatIndex, GX_VA_TEX7, static_cast<GXCompType>(GetRegValue(value, 3, 24)));
                    gxState.SetVertexFormatFraction(formatIndex, GX_VA_TEX7, static_cast<u8>(GetRegValue(value, 5, 27)));
                }
            }
            break;
    }
}

}

static SIM::GX::CommandProcessor * sCommandProcessor;
static u8* sDisplayListBuffer;
static size_t sDisplayListCapacity;
static size_t sDisplayListSize;
static bool sDisplayListRecording;
static bool sDisplayListOverflow;

static void SendCommandProcessorData(const void* data, size_t size) {
    if (sDisplayListRecording) {
        if (!sDisplayListOverflow &&
            size <= sDisplayListCapacity - sDisplayListSize) {
            const u8* source = static_cast<const u8*>(data);
            for (size_t byte = 0; byte < size; ++byte) {
                sDisplayListBuffer[sDisplayListSize + byte] =
                    source[size - byte - 1];
            }
            sDisplayListSize += size;
        } else {
            sDisplayListOverflow = true;
        }
        return;
    }

    sCommandProcessor->ProcessFifoData(
        static_cast<u8*>(const_cast<void*>(data)), size);
}

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
    SendCommandProcessorData(&data, sizeof(data));
}

void SIM_GX_CommandProcessor_SendU16(u16 data) {
    SendCommandProcessorData(&data, sizeof(data));
}

void SIM_GX_CommandProcessor_SendS16(s16 data) {
    SendCommandProcessorData(&data, sizeof(data));
}

void SIM_GX_CommandProcessor_SendU32(u32 data) {
    SendCommandProcessorData(&data, sizeof(data));
}

void SIM_GX_CommandProcessor_SendF32(f32 data) {
    SendCommandProcessorData(&data, sizeof(data));
}

void SIM_GX_CommandProcessor_SendU64(u64 data) {
    SendCommandProcessorData(&data, sizeof(data));
}

GXBool SIM_GX_CommandProcessor_BeginDisplayList(void* list, u32 size) {
    if (list == nullptr || size == 0 || sDisplayListRecording) {
        return GX_FALSE;
    }

    sDisplayListBuffer = static_cast<u8*>(list);
    sDisplayListCapacity = size;
    sDisplayListSize = 0;
    sDisplayListOverflow = false;
    sDisplayListRecording = true;
    return GX_TRUE;
}

u32 SIM_GX_CommandProcessor_EndDisplayList(void) {
    if (!sDisplayListRecording) {
        return 0;
    }

    const size_t alignedSize = (sDisplayListSize + 31u) & ~size_t(31u);
    if (alignedSize > sDisplayListCapacity) {
        sDisplayListOverflow = true;
    } else if (!sDisplayListOverflow) {
        std::memset(
            sDisplayListBuffer + sDisplayListSize,
            0,
            alignedSize - sDisplayListSize);
    }

    const u32 result =
        sDisplayListOverflow ? 0 : static_cast<u32>(alignedSize);
    sDisplayListBuffer = nullptr;
    sDisplayListCapacity = 0;
    sDisplayListSize = 0;
    sDisplayListRecording = false;
    sDisplayListOverflow = false;
    return result;
}

void SIM_GX_CommandProcessor_CallDisplayList(const void* list, u32 size) {
    if (list == nullptr || size == 0 || sDisplayListRecording) {
        return;
    }

    sCommandProcessor->ProcessDisplayList(
        static_cast<const u8*>(list), size);
}

void SIM_GX_CommandProcessor_SetVertexArray(GXAttr attr, void * ptr, int stride) {
    SIM::GX::VertexArray vtxArray;
    vtxArray.mArrayPtr = ptr;
    vtxArray.mStride = stride;
    SIM::GX::GetGlobalState().SetVertexArray(attr, vtxArray);
}

void SIM_GX_CommandProcessor_SetVertexArrayU32(
    GXAttr attr, void * ptr, int stride) {
    SIM::GX::VertexArray vtxArray;
    vtxArray.mArrayPtr = ptr;
    vtxArray.mStride = stride;
    vtxArray.mHostPackedU32 = true;
    SIM::GX::GetGlobalState().SetVertexArray(attr, vtxArray);
}

void SIM_GX_CommandProcessor_LoadTlut(
    u32 id, const void* data, u32 format, u16 entries) {
    SIM::GX::TlutState tlut;
    tlut.data = data;
    tlut.format = static_cast<GXTlutFmt>(format);
    tlut.entries = entries;
    SIM::GX::GetGlobalState().LoadTlut(id, tlut);
}

void SIM_GX_CommandProcessor_LoadTexture(
    u32 id, const void* data, u16 width, u16 height, u32 format,
    u32 wrap_s, u32 wrap_t, u32 min_filter, u32 mag_filter,
    u32 tlut_name) {
    SIM::GX::TextureState texture;
    texture.data = data;
    texture.width = width;
    texture.height = height;
    texture.format = static_cast<GXTexFmt>(format);
    texture.wrapS = static_cast<GXTexWrapMode>(wrap_s);
    texture.wrapT = static_cast<GXTexWrapMode>(wrap_t);
    texture.minFilter = static_cast<GXTexFilter>(min_filter);
    texture.magFilter = static_cast<GXTexFilter>(mag_filter);
    texture.tlutName = tlut_name;
    SIM::GX::GetGlobalState().LoadTexture(id, texture);
}
