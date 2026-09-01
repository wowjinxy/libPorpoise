#include "dolphin/gx/GXEnum.h"
#include <simulator/sim_gx_State.hpp>

#include <algorithm>
#include <cstring>

#include <cmath>

static SIM::GX::GlobalState sGXGlobalState = {};


static u32 GetRegValue(u32 reg, u32 size, u32 shift) {
  return (reg >> shift) & ((1u << size) - 1u);
}

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

    auto HandleAttribute = [this, format](GXAttr attr, bool colorType, size_t (*numComponentsFunc)(GXCompCnt)) -> size_t {

            auto descriptor = mVertexDescriptors[attr];
            if(descriptor == GX_NONE) {
                return 0;
            }

            size_t components = 1;
            if(descriptor == GX_DIRECT) {
                components = numComponentsFunc(format.mAttributes[attr].mComponents);
            }

            return components * GetDescriptorSize(descriptor, format.mAttributes[attr].mDataType, colorType);
    };

    totalBytes += HandleAttribute(GX_VA_PNMTXIDX, false, GetNumMtxIdxComponents);
    for(int i=GX_VA_TEX0MTXIDX; i <= GX_VA_TEX7MTXIDX; i++) {
        totalBytes += HandleAttribute(static_cast<GXAttr>(i), false, GetNumMtxIdxComponents);
    }
    totalBytes += HandleAttribute(GX_VA_POS, false, GetNumPositionComponents);
    totalBytes += HandleAttribute(GX_VA_NRM, false, GetNumNormalComponents);
    totalBytes += HandleAttribute(GX_VA_CLR0, true, GetNumColorComponents);
    totalBytes += HandleAttribute(GX_VA_CLR1, true, GetNumColorComponents);

    for(int attrib = GX_VA_TEX0; attrib <= GX_VA_TEX7; attrib++) {
        totalBytes += HandleAttribute(static_cast<GXAttr>(attrib), false, GetNumTexCoordComponents);
    }

    totalBytes += HandleAttribute(GX_VA_NBT, false, GetNumNBTComponents);

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

const std::array<float, 16>& GlobalState::GetTextureMatrix(int id) const {
    if(mTextureMatrixValid[id]) {
        return mTextureMatrices[id];
    }

    static const std::array<float, 16> identity = IdentityMatrix();
    return identity;
}

void GlobalState::SetXfData(u32 address, const u8* data, size_t wordCount) {
    if (data == nullptr || wordCount == 0 || address >= mXfMemory.size()) {
        return;
    }

    const u32* dataWords = (const u32*)data;

    if(address < 0x1000) {
        // XF memory
        //if(address < 0x78) {
        //    //Position matrices
        //} else if (address < 0xF0) {
        //    // Texture matrices
        //} else if(address >= 0x400 && address < 0x45A) {
        //    // Normal matrices
        //} else if(address >= 0x500 && address < 0x5F0) {
        //    // Post transform texture matrices
        //} else if(address >= 0x600 && address < 0x680) {
        //    // Lights
        //}
        const size_t writableWords =
            std::min(wordCount, mXfMemory.size() - static_cast<size_t>(address));

        for (size_t i = 0; i < writableWords; ++i) {
            std::memcpy(&mXfMemory[address + i], data + i * sizeof(u32), sizeof(u32));
        }

        const u32 endAddress = address + static_cast<u32>(writableWords);
        RefreshPositionMatrices(address, endAddress);
        RefreshTextureMatrices(address, endAddress);
        RefreshLights(address, endAddress);
    } else {
        // XF Registers
        u32 regAddr = address - 0x1000;


        for(u32 i = 0; i< wordCount; i++) {
            const u32 reg = regAddr + i;


            if(reg == 0x09) {
                // Num Chans
                mNumChannels = dataWords[i];
            } else if (reg >= 0x0A && reg <= 0x0D) {
                // Channel color
                const u32 channelNo = (reg - 0x0A) & 1;

                u8* colorPtr = (u8*)(&dataWords[i]);
                if(reg <= 0x0B) {
                    mColorChannels[GX_COLOR0 + channelNo].mAmbientColor[0] = (float)(colorPtr[0]) / 255.0f;
                    mColorChannels[GX_COLOR0 + channelNo].mAmbientColor[1] = (float)(colorPtr[1]) / 255.0f;
                    mColorChannels[GX_COLOR0 + channelNo].mAmbientColor[2] = (float)(colorPtr[2]) / 255.0f;
                    mColorChannels[GX_ALPHA0 + channelNo].mAmbientColor[3] = (float)(colorPtr[3]) / 255.0f;
                    mColorChannels[GX_ALPHA0 + channelNo].mAmbientColor[0] = (float)(colorPtr[0]) / 255.0f;
                    mColorChannels[GX_ALPHA0 + channelNo].mAmbientColor[1] = (float)(colorPtr[1]) / 255.0f;
                    mColorChannels[GX_ALPHA0 + channelNo].mAmbientColor[2] = (float)(colorPtr[2]) / 255.0f;
                    mColorChannels[GX_ALPHA0 + channelNo].mAmbientColor[3] = (float)(colorPtr[3]) / 255.0f;
                } else {
                    mColorChannels[GX_COLOR0 + channelNo].mMaterialColor[0] = (float)(colorPtr[0]) / 255.0f;
                    mColorChannels[GX_COLOR0 + channelNo].mMaterialColor[1] = (float)(colorPtr[1]) / 255.0f;
                    mColorChannels[GX_COLOR0 + channelNo].mMaterialColor[2] = (float)(colorPtr[2]) / 255.0f;
                    mColorChannels[GX_ALPHA0 + channelNo].mMaterialColor[3] = (float)(colorPtr[3]) / 255.0f;
                    mColorChannels[GX_ALPHA0 + channelNo].mMaterialColor[0] = (float)(colorPtr[0]) / 255.0f;
                    mColorChannels[GX_ALPHA0 + channelNo].mMaterialColor[1] = (float)(colorPtr[1]) / 255.0f;
                    mColorChannels[GX_ALPHA0 + channelNo].mMaterialColor[2] = (float)(colorPtr[2]) / 255.0f;
                    mColorChannels[GX_ALPHA0 + channelNo].mMaterialColor[3] = (float)(colorPtr[3]) / 255.0f;
                }
            } else if(reg >= 0x0E && reg <= 0x11) {
                // Channel Control
                u32 chanId = reg - 0x0E;
                if (chanId >= 4) {
                  return;
                }

                auto& channel = mColorChannels[chanId];
                u32 lightsLo = GetRegValue(dataWords[i], 4, 2);
                u32 lightsHi = GetRegValue(dataWords[i], 4, 11);
                channel.mLightMask = (lightsLo | (lightsHi << 4));

                channel.mMaterialSource = static_cast<GXColorSrc>(GetRegValue(dataWords[i], 1, 0));
                channel.mLightingEnabled = GetRegValue(dataWords[i], 1, 1) != 0;
                channel.mAmbientSource = static_cast<GXColorSrc>(GetRegValue(dataWords[i], 1, 6));
                channel.mDiffuseFunction = static_cast<GXDiffuseFn>(GetRegValue(dataWords[i], 2, 7));
                // bit 9 = (attnFn != GX_AF_NONE), bit 10 = (attnFn != GX_AF_SPEC)
                bool bit9 = GetRegValue(dataWords[i], 1, 9) != 0;
                bool bit10 = GetRegValue(dataWords[i], 1, 10) != 0;
                if (!bit10) {
                  channel.mAttnFunction = GX_AF_SPEC;
                } else if (!bit9) {
                  channel.mAttnFunction = GX_AF_NONE;
                } else {
                  channel.mAttnFunction = GX_AF_SPOT;
                }
            } else if (reg == 0x20 && wordCount - i >= 7) {
                // Projection matrix
                const float p0 = WordToFloat(dataWords[i]);
                const float p1 = WordToFloat(dataWords[i+1]);
                const float p2 = WordToFloat(dataWords[i+2]);
                const float p3 = WordToFloat(dataWords[i+3]);
                const float p4 = WordToFloat(dataWords[i+4]);
                const float p5 = WordToFloat(dataWords[i+5]);

                const bool orthographic = dataWords[i+6] == GX_ORTHOGRAPHIC;
                        
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

                i += 6;
            } else if(reg >= 0x40 && reg <= 0x4F) {
                // TexGen config
                u32 texGenIdx = reg - 0x40;
                u32 value = dataWords[i];
                if(texGenIdx >= GX_MAX_TEXCOORD) {
                    continue;
                }
            
                auto& texGenConfig = mTexGenConfigs[texGenIdx];
                bool proj = GetRegValue(value, 1, 1) != 0;
                u32 tgType = GetRegValue(value, 3, 4);
                u32 srcRow = GetRegValue(value, 5, 7);
            
                if (tgType == 0) {
                  texGenConfig.mType = proj ? GX_TG_MTX3x4 : GX_TG_MTX2x4;
                } else if (tgType == 1) {
                  // Bump mapping: type encodes emboss light
                  texGenConfig.mType = static_cast<GXTexGenType>(GetRegValue(value, 3, 15) + 2);
                } else if (tgType == 2 || tgType == 3) {
                  texGenConfig.mType = GX_TG_SRTG;
                  //tcg.src = tgType == 2 ? GX_TG_COLOR0 : GX_TG_COLOR1;
                }
            }
        }
    }


}

void GlobalState::SetTevColor(u8 reg, std::array<float, 4>& color) {
    mInitialTevColors[reg] = color;
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

void GlobalState::RefreshTextureMatrices(u32 firstAddress, u32 endAddress) {
    constexpr u32 wordsPerMatrix = 12;
    for (size_t slot = 0; slot < mTextureMatrices.size(); ++slot) {
        const u32 matrixStart = static_cast<u32>(slot) * wordsPerMatrix + 0x078;
        const u32 matrixEnd = matrixStart + wordsPerMatrix;
        if (endAddress <= matrixStart || firstAddress >= matrixEnd) {
            continue;
        }

        auto& matrix = mTextureMatrices[slot];
        matrix = IdentityMatrix();
        for (size_t row = 0; row < 3; ++row) {
            for (size_t column = 0; column < 4; ++column) {
                const size_t source = matrixStart + row * 4 + column;
                matrix[row * 4 + column] = WordToFloat(mXfMemory[source]);
            }
        }
        mTextureMatrixValid[slot] = true;
    }
}

void GlobalState::RefreshLights(u32 firstAddress, u32 endAddress) {
    for(auto lightId = 0; lightId < GX_MAX_LIGHT; lightId++) {
        u32 lightAddr = 0x600 + (lightId * 0x10);
        if((firstAddress > lightAddr + 0x10) || (endAddress < lightAddr)) {
            continue;
        }
        auto& light = mLights[lightId];
        u32 * lightXfPtr = &mXfMemory[lightAddr];

        // Color
        u8 * lightColorPtr = (u8*)(lightXfPtr+3);
        light.mColor[0] = (float)lightColorPtr[0] / 255.0f;
        light.mColor[1] = (float)lightColorPtr[1] / 255.0f;
        light.mColor[2] = (float)lightColorPtr[2] / 255.0f;
        light.mColor[3] = (float)lightColorPtr[3] / 255.0f;

        // CosAtt
        light.mCosAtt[0] = *(float*)(lightXfPtr+4);
        light.mCosAtt[1] = *(float*)(lightXfPtr+5);
        light.mCosAtt[2] = *(float*)(lightXfPtr+6);

        // DistAtt
        light.mDistAtt[0] = *(float*)(lightXfPtr+7);
        light.mDistAtt[1] = *(float*)(lightXfPtr+8);
        light.mDistAtt[2] = *(float*)(lightXfPtr+9);

        // Position
        light.mPosition[0] = *(float*)(lightXfPtr+10);
        light.mPosition[1] = *(float*)(lightXfPtr+11);
        light.mPosition[2] = *(float*)(lightXfPtr+12);

        // Direction
        light.mDirection[0] = *(float*)(lightXfPtr+13);
        light.mDirection[1] = *(float*)(lightXfPtr+14);
        light.mDirection[2] = *(float*)(lightXfPtr+15);
    }
}

void GlobalState::AddNativeEndianDisplayList(void * displayListPtr) {
    mNativeEndianDisplayLists.emplace(displayListPtr);
}

bool GlobalState::IsDisplayListNativeEndian(void * displayListPtr) {
    return mNativeEndianDisplayLists.contains(displayListPtr);
}

void InitGlobalState() {
    sGXGlobalState.Reset();
}

GlobalState& GetGlobalState() {
    return sGXGlobalState;
}
}
