#include <simulator/sim_gx_State.hpp>

#include <algorithm>
#include <cstring>

static SIM::GX::GlobalState sGXGlobalState ={};

namespace SIM::GX {

GlobalState::GlobalState() {
    Reset();
}

std::array<float, 16> GlobalState::IdentityMatrix() {
    return {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
}

float GlobalState::WordToFloat(u32 word) {
    float value;
    static_assert(sizeof(value) == sizeof(word));
    std::memcpy(&value, &word, sizeof(value));
    return value;
}

void GlobalState::Reset() {
    mCurrentVertexFormat = GX_VTXFMT0;
    mCurrentPrimitive = GX_TRIANGLES;
    mCurrentPositionMatrix = 0;
    mVertexDescriptors.fill(GX_NONE);
    mVertexFormats = {};
    mVertexArrays = {};
    mXfMemory.fill(0);
    mPositionMatrixValid.fill(false);
    for (auto& matrix : mPositionMatrices) {
        matrix = IdentityMatrix();
    }
    mProjectionMatrix = IdentityMatrix();
    mProjectionMatrixValid = false;
}

size_t GlobalState::GetDescriptorSize(GXAttrType descriptorType, GXCompType dataType, bool isColorType) {
    switch(descriptorType) {
        case GX_DIRECT:
            if(isColorType) {
                switch(dataType) {
                    case GX_RGB565:
                    case GX_RGBA4:
                        return 2;
                    case GX_RGB8:
                    case GX_RGBA6:
                        return 3;
                    case GX_RGBX8:
                    case GX_RGBA8:
                        return 4;
                    default:
                        return 0;
                }
            } else {
                switch(dataType) {
                    case GX_U8:
                        return sizeof(u8);
                    case GX_S8:
                        return sizeof(s8);
                    case GX_U16:
                        return sizeof(u16);
                    case GX_S16:
                        return sizeof(s16);
                    case GX_F32:
                        return sizeof(f32);
                    default:
                        return 0;
                }
            }

            break;
        case GX_INDEX8:
            return sizeof(u8);
        case GX_INDEX16:
            return sizeof(u16);
        default:
        case GX_NONE:
            return 0;
    }
}

size_t GlobalState::GetNumBytesPerVertex() {
    size_t totalBytes = 0;
    auto& format = mVertexFormats[mCurrentVertexFormat];




    // Get bytes from position (vtx attribute 9)
    size_t posComponents = 1;
    if(mVertexDescriptors[GX_VA_POS] == GX_DIRECT) {
        posComponents = format.mAttributes[GX_VA_POS].mComponents == GX_POS_XYZ ? 3 : 2;
    }

    totalBytes += posComponents * GetDescriptorSize(mVertexDescriptors[GX_VA_POS], format.mAttributes[GX_VA_POS].mDataType);

    // Get bytes from color0 (vtx attribute 11)
    totalBytes += GetDescriptorSize(mVertexDescriptors[GX_VA_CLR0], format.mAttributes[GX_VA_CLR0].mDataType, true);

    

    return totalBytes;
}

const std::array<float, 16>& GlobalState::GetPositionMatrix() const {
    if (mCurrentPositionMatrix < mPositionMatrices.size() &&
        mPositionMatrixValid[mCurrentPositionMatrix]) {
        return mPositionMatrices[mCurrentPositionMatrix];
    }

    static const std::array<float, 16> identity = IdentityMatrix();
    return identity;
}

const std::array<float, 16>& GlobalState::GetProjectionMatrix() const {
    if (mProjectionMatrixValid) {
        return mProjectionMatrix;
    }

    static const std::array<float, 16> identity = IdentityMatrix();
    return identity;
}

void GlobalState::SetXfData(u32 address, const u8* data, size_t wordCount) {
    if (data == nullptr || wordCount == 0 || address >= mXfMemory.size()) {
        return;
    }

    const size_t writableWords =
        std::min(wordCount, mXfMemory.size() - static_cast<size_t>(address));
    for (size_t i = 0; i < writableWords; ++i) {
        std::memcpy(&mXfMemory[address + i], data + i * sizeof(u32), sizeof(u32));
    }

    const u32 endAddress = address + static_cast<u32>(writableWords);
    RefreshPositionMatrices(address, endAddress);
    RefreshProjectionMatrix(address, endAddress);
}

void GlobalState::RefreshPositionMatrices(u32 firstAddress, u32 endAddress) {
    constexpr u32 wordsPerMatrix = 12;
    for (size_t slot = 0; slot < mPositionMatrices.size(); ++slot) {
        const u32 matrixStart = static_cast<u32>(slot) * wordsPerMatrix;
        const u32 matrixEnd = matrixStart + wordsPerMatrix;
        if (endAddress <= matrixStart || firstAddress >= matrixEnd) {
            continue;
        }

        auto& matrix = mPositionMatrices[slot];
        matrix = IdentityMatrix();
        for (size_t row = 0; row < 3; ++row) {
            for (size_t column = 0; column < 4; ++column) {
                const size_t source = matrixStart + row * 4 + column;
                matrix[row * 4 + column] = WordToFloat(mXfMemory[source]);
            }
        }
        mPositionMatrixValid[slot] = true;
    }
}

void GlobalState::RefreshProjectionMatrix(u32 firstAddress, u32 endAddress) {
    constexpr u32 projectionStart = 0x1020;
    constexpr u32 projectionEnd = projectionStart + 7;
    if (endAddress <= projectionStart || firstAddress >= projectionEnd) {
        return;
    }

    const float p0 = WordToFloat(mXfMemory[0x1020]);
    const float p1 = WordToFloat(mXfMemory[0x1021]);
    const float p2 = WordToFloat(mXfMemory[0x1022]);
    const float p3 = WordToFloat(mXfMemory[0x1023]);
    const float p4 = WordToFloat(mXfMemory[0x1024]);
    const float p5 = WordToFloat(mXfMemory[0x1025]);
    const bool orthographic = mXfMemory[0x1026] == GX_ORTHOGRAPHIC;

    mProjectionMatrix.fill(0.0f);
    mProjectionMatrix[0] = p0;
    mProjectionMatrix[5] = p2;
    mProjectionMatrix[10] = p4;
    mProjectionMatrix[11] = p5;
    if (orthographic) {
        mProjectionMatrix[3] = p1;
        mProjectionMatrix[7] = p3;
        mProjectionMatrix[15] = 1.0f;
    } else {
        mProjectionMatrix[2] = p1;
        mProjectionMatrix[6] = p3;
        mProjectionMatrix[14] = -1.0f;
    }
    mProjectionMatrixValid = true;
}

void InitGlobalState() {
    sGXGlobalState.Reset();
}

GlobalState& GetGlobalState() {
    return sGXGlobalState;
}
}
