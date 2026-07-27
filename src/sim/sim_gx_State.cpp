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

static std::array<float, 4> DecodeXfColor(u32 word) {
    return {
        static_cast<float>((word >> 24u) & 0xffu) / 255.0f,
        static_cast<float>((word >> 16u) & 0xffu) / 255.0f,
        static_cast<float>((word >> 8u) & 0xffu) / 255.0f,
        static_cast<float>(word & 0xffu) / 255.0f,
    };
}

static ChannelControlState DecodeChannelControl(u32 word) {
    ChannelControlState control;
    control.materialSource =
        (word & (1u << 0u)) != 0u ? GX_SRC_VTX : GX_SRC_REG;
    control.lightingEnabled = (word & (1u << 1u)) != 0u;
    control.ambientSource =
        (word & (1u << 6u)) != 0u ? GX_SRC_VTX : GX_SRC_REG;
    control.lightMask = static_cast<u8>(
        ((word >> 2u) & 0x0fu) |
        (((word >> 11u) & 0x0fu) << 4u));
    control.diffuseFunction =
        static_cast<GXDiffuseFn>((word >> 7u) & 0x03u);

    const bool attenuationEnabled = (word & (1u << 9u)) != 0u;
    const bool notSpecular = (word & (1u << 10u)) != 0u;
    if (!notSpecular) {
        control.attenuationFunction = GX_AF_SPEC;
    } else if (attenuationEnabled) {
        control.attenuationFunction = GX_AF_SPOT;
    } else {
        control.attenuationFunction = GX_AF_NONE;
    }
    return control;
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
    mTextureMatrixValid.fill(false);
    mTextureMatrix2x4.fill(false);
    for (auto& matrix : mTextureMatrices) {
        matrix = IdentityMatrix();
    }
    mTexCoordGens = {};
    mProjectionMatrix = IdentityMatrix();
    mProjectionMatrixValid = false;
    mViewportTransform.fill(0.0f);
    mViewportTransformValid = false;
    mViewportState = {};
    mScissorState = {};
    mScissorTopLeft = 0;
    mScissorBottomRight = 0;
    mCopySourceLeft = 0;
    mCopySourceTop = 0;
    mCopySourceWidth = 640;
    mCopySourceHeight = 480;
    mCopySourceValid = false;
    mBlendState = {};
    mDepthState = {};
    mZTextureState = {};
    mAlphaCompareState = {};
    mFogState = {};
    mRasterState = {};
    mChannels = {};
    mLights = {};
    mTextures = {};
    mTevStages = {};
    for (auto& table : mTevSwapTables) {
        table = {
            GX_CH_RED,
            GX_CH_GREEN,
            GX_CH_BLUE,
            GX_CH_ALPHA,
        };
    }
    mTevColors = {};
    mNumTevStages = 1;
    mTextureRevision = 0;
    mCopyClearColor = {
        64.0f / 255.0f,
        64.0f / 255.0f,
        64.0f / 255.0f,
        1.0f,
    };
    mCopyClearDepth = 1.0f;
    mCopyClearRequested = false;
}

size_t GlobalState::GetDescriptorSize(GXAttrType descriptorType, GXCompType dataType, bool isColorType) const {
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

size_t GlobalState::GetVertexAttributeInputSize(GXAttr attr) const {
    if (attr < GX_VA_PNMTXIDX || attr > GX_VA_TEX7) {
        return 0;
    }

    const GXAttrType descriptor = mVertexDescriptors[attr];
    if (descriptor == GX_NONE) {
        return 0;
    }

    if (attr <= GX_VA_TEX7MTXIDX) {
        return sizeof(u8);
    }

    const auto& attributes = mVertexFormats[mCurrentVertexFormat].mAttributes[attr];
    if (descriptor == GX_INDEX8 || descriptor == GX_INDEX16) {
        size_t indexSize =
            descriptor == GX_INDEX8 ? sizeof(u8) : sizeof(u16);
        if (attr == GX_VA_NRM && attributes.mComponents == GX_NRM_NBT3) {
            indexSize *= 3;
        }
        return indexSize;
    }

    if (descriptor != GX_DIRECT) {
        return 0;
    }

    if (attr == GX_VA_CLR0 || attr == GX_VA_CLR1) {
        return GetDescriptorSize(GX_DIRECT, attributes.mDataType, true);
    }

    size_t componentCount = 0;
    if (attr == GX_VA_POS) {
        componentCount = GetNumPositionComponents(attributes.mComponents);
    } else if (attr == GX_VA_NRM) {
        componentCount =
            attributes.mComponents == GX_NRM_XYZ ? 3 : 9;
    } else if (attr >= GX_VA_TEX0 && attr <= GX_VA_TEX7) {
        componentCount =
            attributes.mComponents == GX_TEX_ST ? 2 : 1;
    }

    return componentCount *
           GetDescriptorSize(GX_DIRECT, attributes.mDataType);
}

size_t GlobalState::GetNumBytesPerVertex() const {
    size_t totalBytes = 0;
    for (u32 attr = GX_VA_PNMTXIDX; attr <= GX_VA_TEX7; ++attr) {
        totalBytes += GetVertexAttributeInputSize(static_cast<GXAttr>(attr));
    }

    return totalBytes;
}

size_t GlobalState::GetNumPositionComponents(GXCompCnt compType) const {
    switch(compType) {
        case GX_POS_XY:
            return 2;
        default:
        case GX_POS_XYZ:
            return 3;
    }
}

GXPrimitive GlobalState::GetCurrentPrimitive() const {
    return mCurrentPrimitive;
}

const VertexArray& GlobalState::GetVertexArray(GXAttr attr) const {
    if (attr == GX_VA_NBT) {
        attr = GX_VA_NRM;
    }
    if (attr < GX_VA_PNMTXIDX || attr >= GX_VA_MAX_ATTR) {
        static const VertexArray empty = {};
        return empty;
    }
    return mVertexArrays[attr];
}

GXAttrType GlobalState::GetVertexDescriptor(GXAttr attr) const {
    return mVertexDescriptors[attr];
}

const VertexFormat& GlobalState::GetCurrentVertexFormat() const {
    return mVertexFormats[mCurrentVertexFormat];
}

const VertexFormat& GlobalState::GetVertexFormat(GXVtxFmt formatIdx) const {
    return mVertexFormats[formatIdx];
}

const std::array<float, 16>& GlobalState::GetPositionMatrix() const {
    return GetPositionMatrix(mCurrentPositionMatrix);
}

const std::array<float, 16>& GlobalState::GetPositionMatrix(
    size_t index) const {
    if (index < mPositionMatrices.size() &&
        mPositionMatrixValid[index]) {
        return mPositionMatrices[index];
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

const std::array<float, 16>& GlobalState::GetTextureMatrix(size_t index) const {
    if (index < mTextureMatrices.size() && mTextureMatrixValid[index]) {
        return mTextureMatrices[index];
    }

    static const std::array<float, 16> identity = IdentityMatrix();
    return identity;
}

const std::array<float, 6>& GlobalState::GetViewportTransform() const {
    return mViewportTransform;
}

const ViewportState& GlobalState::GetViewportState() const {
    return mViewportState;
}

const ScissorState& GlobalState::GetScissorState() const {
    return mScissorState;
}

const BlendState& GlobalState::GetBlendState() const {
    return mBlendState;
}

const DepthState& GlobalState::GetDepthState() const {
    return mDepthState;
}

const ZTextureState& GlobalState::GetZTextureState() const {
    return mZTextureState;
}

const AlphaCompareState& GlobalState::GetAlphaCompareState() const {
    return mAlphaCompareState;
}

const FogState& GlobalState::GetFogState() const {
    return mFogState;
}

const RasterState& GlobalState::GetRasterState() const {
    return mRasterState;
}

const ChannelState& GlobalState::GetChannelState(size_t index) const {
    if (index < mChannels.size()) {
        return mChannels[index];
    }
    static const ChannelState empty = {};
    return empty;
}

const LightState& GlobalState::GetLightState(size_t index) const {
    if (index < mLights.size()) {
        return mLights[index];
    }
    static const LightState empty = {};
    return empty;
}

const TextureState& GlobalState::GetTextureState(size_t index) const {
    if (index < mTextures.size()) {
        return mTextures[index];
    }
    static const TextureState empty = {};
    return empty;
}

const TexCoordGenState& GlobalState::GetTexCoordGenState(size_t index) const {
    if (index < mTexCoordGens.size()) {
        return mTexCoordGens[index];
    }
    static const TexCoordGenState empty = {};
    return empty;
}

const std::array<float, 16>& GlobalState::GetTexCoordGenMatrix(
    size_t index) const {
    const auto& texGen = GetTexCoordGenState(index);
    if (texGen.matrixId >= GX_TEXMTX0 &&
        texGen.matrixId <= GX_TEXMTX9 &&
        ((texGen.matrixId - GX_TEXMTX0) % 3u) == 0u) {
        return GetTextureMatrix(
            static_cast<size_t>(
                (texGen.matrixId - GX_TEXMTX0) / 3u));
    }

    static const std::array<float, 16> identity = IdentityMatrix();
    return identity;
}

const TevStageState& GlobalState::GetTevStageState(size_t index) const {
    if (index < mTevStages.size()) {
        return mTevStages[index];
    }
    static const TevStageState empty = {};
    return empty;
}

const std::array<u8, 4>& GlobalState::GetTevSwapTable(
    size_t index) const {
    if (index < mTevSwapTables.size()) {
        return mTevSwapTables[index];
    }
    static const std::array<u8, 4> identity = {
        GX_CH_RED,
        GX_CH_GREEN,
        GX_CH_BLUE,
        GX_CH_ALPHA,
    };
    return identity;
}

const std::array<float, 4>& GlobalState::GetTevColor(size_t index) const {
    if (index < mTevColors.size()) {
        return mTevColors[index];
    }
    static const std::array<float, 4> empty = {};
    return empty;
}

size_t GlobalState::GetNumTevStages() const {
    return mNumTevStages;
}

const std::array<float, 4>& GlobalState::GetCopyClearColor() const {
    return mCopyClearColor;
}

float GlobalState::GetCopyClearDepth() const {
    return mCopyClearDepth;
}

bool GlobalState::HasViewportTransform() const {
    return mViewportTransformValid;
}

bool GlobalState::ConsumeCopyClearRequest() {
    const bool requested = mCopyClearRequested;
    mCopyClearRequested = false;
    return requested;
}

void GlobalState::SetBpRegister(u32 registerValue) {
    const u8 address = static_cast<u8>(registerValue >> 24);
    const u32 value = registerValue & 0x00ffffffu;
    const auto field = [value](u32 width, u32 shift) {
        return (value >> shift) & ((1u << width) - 1u);
    };

    switch (address) {
        case 0x00: {
            mNumTevStages = static_cast<size_t>(field(4, 10)) + 1u;
            const auto hardwareMode =
                static_cast<GXCullMode>(field(2, 14));
            switch (hardwareMode) {
                case GX_CULL_FRONT:
                    mRasterState.cullMode = GX_CULL_BACK;
                    break;
                case GX_CULL_BACK:
                    mRasterState.cullMode = GX_CULL_FRONT;
                    break;
                default:
                    mRasterState.cullMode = hardwareMode;
                    break;
            }
            break;
        }
        case 0x20:
            mScissorTopLeft = value;
            if (mScissorBottomRight != 0u) {
                const u32 encodedTop = mScissorTopLeft & 0x7ffu;
                const u32 encodedLeft =
                    (mScissorTopLeft >> 12u) & 0x7ffu;
                const u32 encodedBottom =
                    mScissorBottomRight & 0x7ffu;
                const u32 encodedRight =
                    (mScissorBottomRight >> 12u) & 0x7ffu;
                if (encodedRight >= encodedLeft &&
                    encodedBottom >= encodedTop) {
                    mScissorState.left =
                        encodedLeft >= 342u ? encodedLeft - 342u : 0u;
                    mScissorState.top =
                        encodedTop >= 342u ? encodedTop - 342u : 0u;
                    mScissorState.width =
                        encodedRight - encodedLeft + 1u;
                    mScissorState.height =
                        encodedBottom - encodedTop + 1u;
                    mScissorState.valid = true;
                }
            }
            break;
        case 0x21:
            mScissorBottomRight = value;
            if (mScissorTopLeft != 0u) {
                const u32 encodedTop = mScissorTopLeft & 0x7ffu;
                const u32 encodedLeft =
                    (mScissorTopLeft >> 12u) & 0x7ffu;
                const u32 encodedBottom =
                    mScissorBottomRight & 0x7ffu;
                const u32 encodedRight =
                    (mScissorBottomRight >> 12u) & 0x7ffu;
                if (encodedRight >= encodedLeft &&
                    encodedBottom >= encodedTop) {
                    mScissorState.left =
                        encodedLeft >= 342u ? encodedLeft - 342u : 0u;
                    mScissorState.top =
                        encodedTop >= 342u ? encodedTop - 342u : 0u;
                    mScissorState.width =
                        encodedRight - encodedLeft + 1u;
                    mScissorState.height =
                        encodedBottom - encodedTop + 1u;
                    mScissorState.valid = true;
                }
            }
            break;
        case 0x28:
        case 0x29:
        case 0x2a:
        case 0x2b:
        case 0x2c:
        case 0x2d:
        case 0x2e:
        case 0x2f: {
            const size_t firstStage =
                static_cast<size_t>(address - 0x28u) * 2u;
            auto& evenStage = mTevStages[firstStage];
            evenStage.textureMap = static_cast<u8>(field(3, 0));
            evenStage.textureCoordinate = static_cast<u8>(field(3, 3));
            evenStage.textureEnabled = field(1, 6) != 0;
            evenStage.rasterChannel = static_cast<u8>(field(3, 7));

            auto& oddStage = mTevStages[firstStage + 1u];
            oddStage.textureMap = static_cast<u8>(field(3, 12));
            oddStage.textureCoordinate = static_cast<u8>(field(3, 15));
            oddStage.textureEnabled = field(1, 18) != 0;
            oddStage.rasterChannel = static_cast<u8>(field(3, 19));
            break;
        }
        case 0x22:
            // GX line and point sizes are stored in sixths of a pixel.
            mRasterState.lineWidth =
                std::max(1.0f, static_cast<float>(field(8, 0)) / 6.0f);
            mRasterState.pointSize =
                std::max(1.0f, static_cast<float>(field(8, 8)) / 6.0f);
            break;
        case 0x40:
            mDepthState.compareEnabled = field(1, 0) != 0;
            mDepthState.function =
                static_cast<GXCompare>(field(3, 1));
            mDepthState.updateEnabled = field(1, 4) != 0;
            break;
        case 0x41: {
            const bool blendEnabled = field(1, 0) != 0;
            const bool logicEnabled = field(1, 1) != 0;
            const bool subtractEnabled = field(1, 11) != 0;
            if (subtractEnabled) {
                mBlendState.mode = GX_BM_SUBTRACT;
            } else if (logicEnabled) {
                mBlendState.mode = GX_BM_LOGIC;
            } else if (blendEnabled) {
                mBlendState.mode = GX_BM_BLEND;
            } else {
                mBlendState.mode = GX_BM_NONE;
            }
            mBlendState.ditherEnabled = field(1, 2) != 0;
            mBlendState.colorUpdateEnabled = field(1, 3) != 0;
            mBlendState.alphaUpdateEnabled = field(1, 4) != 0;
            mBlendState.destinationFactor =
                static_cast<GXBlendFactor>(field(3, 5));
            mBlendState.sourceFactor =
                static_cast<GXBlendFactor>(field(3, 8));
            mBlendState.logicOperation =
                static_cast<GXLogicOp>(field(4, 12));
            break;
        }
        case 0x49:
            mCopySourceLeft = field(10, 0);
            mCopySourceTop = field(10, 10);
            break;
        case 0x4a:
            mCopySourceWidth = field(10, 0) + 1u;
            mCopySourceHeight = field(10, 10) + 1u;
            mCopySourceValid = true;
            break;
        case 0x4f:
            mCopyClearColor[0] =
                static_cast<float>(field(8, 0)) / 255.0f;
            mCopyClearColor[3] =
                static_cast<float>(field(8, 8)) / 255.0f;
            break;
        case 0x50:
            mCopyClearColor[2] =
                static_cast<float>(field(8, 0)) / 255.0f;
            mCopyClearColor[1] =
                static_cast<float>(field(8, 8)) / 255.0f;
            break;
        case 0x51:
            mCopyClearDepth =
                static_cast<float>(field(24, 0)) / 16777215.0f;
            break;
        case 0x52:
            /*
             * A display copy defines the EFB area that is scaled into the
             * XFB. Rebase the host viewport on that area instead of retaining
             * a larger viewport used earlier during GX initialization. For
             * example, NTSC demos commonly render 448 EFB lines and scale
             * them to a 480-line display.
             */
            if (field(1, 14) != 0 && mCopySourceValid) {
                const float viewportRight =
                    mViewportState.left + mViewportState.width;
                const float viewportBottom =
                    mViewportState.top + mViewportState.height;
                const float scissorRight =
                    mScissorState.valid
                        ? static_cast<float>(
                              mScissorState.left + mScissorState.width)
                        : 0.0f;
                const float scissorBottom =
                    mScissorState.valid
                        ? static_cast<float>(
                              mScissorState.top + mScissorState.height)
                        : 0.0f;
                mViewportState.referenceWidth = std::max(
                    {
                        1.0f,
                        static_cast<float>(
                            mCopySourceLeft + mCopySourceWidth),
                        viewportRight,
                        scissorRight,
                    });
                mViewportState.referenceHeight = std::max(
                    {
                        1.0f,
                        static_cast<float>(
                            mCopySourceTop + mCopySourceHeight),
                        viewportBottom,
                        scissorBottom,
                    });
            }
            // Setting the copy trigger with the clear bit clears the EFB after
            // it has been copied, preparing it for the next frame.
            if (field(1, 14) != 0 && field(1, 11) != 0) {
                mCopyClearRequested = true;
            }
            break;
        case 0xe8: {
            const u32 encodedCenter = field(10, 0);
            mFogState.rangeAdjustmentCenter =
                static_cast<u16>(
                    encodedCenter >= 342u
                        ? encodedCenter - 342u
                        : 0u);
            mFogState.rangeAdjustmentEnabled =
                field(1, 10) != 0;
            break;
        }
        case 0xe9:
        case 0xea:
        case 0xeb:
        case 0xec:
        case 0xed: {
            const size_t firstIndex =
                static_cast<size_t>(address - 0xe9u) * 2u;
            mFogState.rangeAdjustmentTable[firstIndex] =
                static_cast<u16>(field(12, 0));
            mFogState.rangeAdjustmentTable[firstIndex + 1u] =
                static_cast<u16>(field(12, 12));
            break;
        }
        case 0xee:
            mFogState.parameterA =
                WordToFloat((value & 0x000fffffu) << 12u);
            break;
        case 0xef:
            mFogState.parameterBMagnitude = field(24, 0);
            break;
        case 0xf0:
            mFogState.parameterBShift =
                static_cast<u8>(field(5, 0));
            break;
        case 0xf1:
            mFogState.parameterC =
                WordToFloat((value & 0x000fffffu) << 12u);
            mFogState.orthographic = field(1, 20) != 0;
            mFogState.type =
                static_cast<GXFogType>(field(3, 21));
            break;
        case 0xf2:
            mFogState.color = {
                static_cast<float>(field(8, 16)) / 255.0f,
                static_cast<float>(field(8, 8)) / 255.0f,
                static_cast<float>(field(8, 0)) / 255.0f,
            };
            break;
        case 0xf3:
            mAlphaCompareState.reference0 =
                static_cast<u8>(field(8, 0));
            mAlphaCompareState.reference1 =
                static_cast<u8>(field(8, 8));
            mAlphaCompareState.comparison0 =
                static_cast<GXCompare>(field(3, 16));
            mAlphaCompareState.comparison1 =
                static_cast<GXCompare>(field(3, 19));
            mAlphaCompareState.operation =
                static_cast<GXAlphaOp>(field(2, 22));
            break;
        case 0xf4:
            mZTextureState.bias = field(24, 0);
            break;
        case 0xf5:
            switch (field(2, 0)) {
                case 0:
                    mZTextureState.format = GX_TF_Z8;
                    break;
                case 1:
                    mZTextureState.format = GX_TF_Z16;
                    break;
                case 2:
                default:
                    mZTextureState.format = GX_TF_Z24X8;
                    break;
            }
            mZTextureState.operation =
                static_cast<GXZTexOp>(field(2, 2));
            break;
        case 0xf6:
        case 0xf7:
        case 0xf8:
        case 0xf9:
        case 0xfa:
        case 0xfb:
        case 0xfc:
        case 0xfd: {
            const size_t registerIndex =
                static_cast<size_t>(address - 0xf6u);
            const size_t tableIndex = registerIndex / 2u;
            const size_t componentOffset =
                (registerIndex & 1u) * 2u;
            auto& table = mTevSwapTables[tableIndex];
            table[componentOffset] =
                static_cast<u8>(field(2, 0));
            table[componentOffset + 1u] =
                static_cast<u8>(field(2, 2));
            break;
        }
        default:
            if (address >= 0xe0u && address <= 0xe7u) {
                const size_t colorIndex =
                    static_cast<size_t>((address - 0xe0u) / 2u);
                auto& color = mTevColors[colorIndex];
                if ((address & 1u) == 0u) {
                    color[0] =
                        static_cast<float>(field(8, 0)) / 255.0f;
                    color[3] =
                        static_cast<float>(field(8, 12)) / 255.0f;
                } else {
                    color[2] =
                        static_cast<float>(field(8, 0)) / 255.0f;
                    color[1] =
                        static_cast<float>(field(8, 12)) / 255.0f;
                }
            }
            if (address >= 0xc0u &&
                address <= 0xdeu &&
                (address & 1u) == 0u) {
                const size_t stage =
                    static_cast<size_t>((address - 0xc0u) / 2u);
                const u32 inputA = field(4, 12);
                const u32 inputB = field(4, 8);
                const u32 inputC = field(4, 4);
                const u32 inputD = field(4, 0);
                const u32 operation =
                    // Bias encoding 3 selects the TEV comparison operations.
                    field(2, 16) == 3u
                        ? 8u | (field(2, 20) << 1u) | field(1, 18)
                        : field(1, 18);
                auto& tevStage = mTevStages[stage];
                tevStage.colorInputs = {
                    static_cast<u8>(inputA),
                    static_cast<u8>(inputB),
                    static_cast<u8>(inputC),
                    static_cast<u8>(inputD),
                };
                tevStage.colorOperation =
                    static_cast<GXTevOp>(operation);
                tevStage.colorBias =
                    field(2, 16) == 3u
                        ? GX_TB_ZERO
                        : static_cast<GXTevBias>(field(2, 16));
                tevStage.colorScale =
                    static_cast<GXTevScale>(field(2, 20));
                tevStage.colorClamp = field(1, 19) != 0u;
                tevStage.colorOutput =
                    static_cast<GXTevRegID>(field(2, 22));
                if (operation == GX_TEV_COMP_RGB8_EQ &&
                    inputA == GX_CC_TEXC &&
                    inputB == GX_CC_ZERO &&
                    inputC == GX_CC_ONE &&
                    inputD == GX_CC_C0) {
                    tevStage.colorMode =
                        TevColorMode::CompareTextureRgb8EqualZero;
                } else if (
                    inputA == GX_CC_ZERO &&
                    inputB == GX_CC_TEXC &&
                    inputC == GX_CC_RASC &&
                    inputD == GX_CC_ZERO) {
                    tevStage.colorMode =
                        TevColorMode::Modulate;
                } else if (
                    inputA == GX_CC_ZERO &&
                    inputB == GX_CC_ZERO &&
                    inputC == GX_CC_ZERO &&
                    inputD == GX_CC_TEXC) {
                    tevStage.colorMode =
                        TevColorMode::ReplaceTexture;
                } else if (
                    inputA == GX_CC_ZERO &&
                    inputB == GX_CC_ZERO &&
                    inputC == GX_CC_ZERO &&
                    inputD == GX_CC_RASC) {
                    tevStage.colorMode =
                        TevColorMode::PassColor;
                }
            }
            if (address >= 0xc1u &&
                address <= 0xdfu &&
                (address & 1u) != 0u) {
                const size_t stage =
                    static_cast<size_t>((address - 0xc1u) / 2u);
                const u32 operation =
                    field(2, 16) == 3u
                        ? 8u | (field(2, 20) << 1u) | field(1, 18)
                        : field(1, 18);
                auto& tevStage = mTevStages[stage];
                tevStage.rasterSwapTable =
                    static_cast<u8>(field(2, 0));
                tevStage.textureSwapTable =
                    static_cast<u8>(field(2, 2));
                tevStage.alphaInputs = {
                    static_cast<u8>(field(3, 13)),
                    static_cast<u8>(field(3, 10)),
                    static_cast<u8>(field(3, 7)),
                    static_cast<u8>(field(3, 4)),
                };
                tevStage.alphaOperation =
                    static_cast<GXTevOp>(operation);
                tevStage.alphaBias =
                    field(2, 16) == 3u
                        ? GX_TB_ZERO
                        : static_cast<GXTevBias>(field(2, 16));
                tevStage.alphaScale =
                    static_cast<GXTevScale>(field(2, 20));
                tevStage.alphaClamp = field(1, 19) != 0u;
                tevStage.alphaOutput =
                    static_cast<GXTevRegID>(field(2, 22));
            }
            break;
    }
}

void GlobalState::SetCurrentPrimitive(GXPrimitive primitive) {
    mCurrentPrimitive = primitive;
}

void GlobalState::SetCurrentPositionMatrix(u32 matrixId) {
    const size_t slot = static_cast<size_t>(matrixId / 3);
    if (slot < mPositionMatrices.size()) {
        mCurrentPositionMatrix = slot;
    }
}

void GlobalState::SetCurrentVertexFormat(GXVtxFmt format) {
    mCurrentVertexFormat = format;
}

void GlobalState::SetVertexArray(GXAttr attr, VertexArray array) {
    if (attr == GX_VA_NBT) {
        attr = GX_VA_NRM;
    }
    if (attr < GX_VA_PNMTXIDX || attr >= GX_VA_MAX_ATTR) {
        return;
    }
    mVertexArrays[attr] = array;
}

void GlobalState::SetVertexDescriptor(GXAttr attr, GXAttrType descType) {
    mVertexDescriptors[attr] = descType;
}

void GlobalState::SetVertexFormatComponents(GXVtxFmt formatIndex, GXAttr attrIndex, GXCompCnt component) {
    mVertexFormats[formatIndex].mAttributes[attrIndex].mComponents = component;
}

void GlobalState::SetVertexFormatDataType(GXVtxFmt formatIndex, GXAttr attrIndex, GXCompType dataType) {
    mVertexFormats[formatIndex].mAttributes[attrIndex].mDataType = dataType;
}

void GlobalState::SetVertexFormatFraction(GXVtxFmt formatIndex, GXAttr attrIndex, u8 fraction) {
    mVertexFormats[formatIndex].mAttributes[attrIndex].mFraction = fraction;
}

void GlobalState::LoadTexture(size_t index, const TextureState& texture) {
    if (index >= mTextures.size()) {
        return;
    }
    mTextures[index] = texture;
    mTextures[index].revision = ++mTextureRevision;
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

    constexpr u32 textureMatrixStart = 30u * 4u;
    constexpr u32 wordsPerTextureMatrix = 12u;
    if ((writableWords == 8u || writableWords == 12u) &&
        address >= textureMatrixStart &&
        (address - textureMatrixStart) % wordsPerTextureMatrix == 0u) {
        const size_t slot = static_cast<size_t>(
            (address - textureMatrixStart) / wordsPerTextureMatrix);
        if (slot < mTextureMatrix2x4.size()) {
            mTextureMatrix2x4[slot] = writableWords == 8u;
        }
    }

    const u32 endAddress = address + static_cast<u32>(writableWords);
    RefreshPositionMatrices(address, endAddress);
    RefreshTextureMatrices(address, endAddress);
    RefreshTexCoordGenState(address, endAddress);
    RefreshProjectionMatrix(address, endAddress);
    RefreshViewportTransform(address, endAddress);
    RefreshChannelState(address, endAddress);
    RefreshLightState(address, endAddress);
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
    constexpr u32 textureMatrixStart = 30u * 4u;
    constexpr u32 wordsPerMatrix = 12;
    for (size_t slot = 0; slot < mTextureMatrices.size(); ++slot) {
        const u32 matrixStart =
            textureMatrixStart + static_cast<u32>(slot) * wordsPerMatrix;
        const u32 matrixEnd = matrixStart + wordsPerMatrix;
        if (endAddress <= matrixStart || firstAddress >= matrixEnd) {
            continue;
        }

        auto& matrix = mTextureMatrices[slot];
        matrix = IdentityMatrix();
        const size_t sourceRows = mTextureMatrix2x4[slot] ? 2u : 3u;
        for (size_t row = 0; row < sourceRows; ++row) {
            for (size_t column = 0; column < 4; ++column) {
                const size_t source = matrixStart + row * 4 + column;
                matrix[row * 4 + column] = WordToFloat(mXfMemory[source]);
            }
        }
        if (mTextureMatrix2x4[slot]) {
            // GX_MTX2x4 produces S and T; its projective Q coordinate is
            // implicitly one rather than a third row read from XF memory.
            matrix[8] = 0.0f;
            matrix[9] = 0.0f;
            matrix[10] = 0.0f;
            matrix[11] = 1.0f;
        }
        mTextureMatrixValid[slot] = true;
    }
}

void GlobalState::RefreshTexCoordGenState(
    u32 firstAddress,
    u32 endAddress) {
    constexpr u32 matrixIndexA = 0x1018;
    constexpr u32 matrixIndexB = 0x1019;
    constexpr u32 texGenStart = 0x1040;
    constexpr u32 texGenEnd = texGenStart + 8;
    const bool matrixIndicesChanged =
        firstAddress < matrixIndexB + 1u && endAddress > matrixIndexA;
    const bool generatorsChanged =
        firstAddress < texGenEnd && endAddress > texGenStart;
    if (!matrixIndicesChanged && !generatorsChanged) {
        return;
    }

    const u32 matrixWords[2] = {
        mXfMemory[matrixIndexA],
        mXfMemory[matrixIndexB],
    };
    for (size_t index = 0; index < mTexCoordGens.size(); ++index) {
        const u32 texGenWord =
            mXfMemory[texGenStart + static_cast<u32>(index)];
        const u32 row = (texGenWord >> 7u) & 0x1fu;
        GXTexGenSrc source = GX_TG_TEX0;
        switch (row) {
            case 0:
                source = GX_TG_POS;
                break;
            case 1:
                source = GX_TG_NRM;
                break;
            case 2:
                source =
                    ((texGenWord >> 4u) & 0x07u) == 3u
                        ? GX_TG_COLOR1
                        : GX_TG_COLOR0;
                break;
            case 3:
                source = GX_TG_BINRM;
                break;
            case 4:
                source = GX_TG_TANGENT;
                break;
            default:
                if (row >= 5u && row <= 12u) {
                    source = static_cast<GXTexGenSrc>(
                        GX_TG_TEX0 + row - 5u);
                }
                break;
        }

        const size_t matrixWord = index < 4u ? 0u : 1u;
        const u32 matrixShift =
            index < 4u
                ? 6u + static_cast<u32>(index) * 6u
                : static_cast<u32>(index - 4u) * 6u;
        mTexCoordGens[index].source = source;
        mTexCoordGens[index].matrixId = static_cast<u8>(
            (matrixWords[matrixWord] >> matrixShift) & 0x3fu);
        const u32 texGenType = (texGenWord >> 4u) & 0x07u;
        if (texGenType == 1u) {
            mTexCoordGens[index].embossSource =
                static_cast<u8>((texGenWord >> 12u) & 0x07u);
            mTexCoordGens[index].embossLight =
                static_cast<u8>((texGenWord >> 15u) & 0x07u);
            mTexCoordGens[index].function =
                static_cast<GXTexGenType>(
                    GX_TG_BUMP0 +
                    mTexCoordGens[index].embossLight);
            mTexCoordGens[index].source =
                static_cast<GXTexGenSrc>(
                    GX_TG_TEXCOORD0 +
                    mTexCoordGens[index].embossSource);
        } else if (texGenType == 2u || texGenType == 3u) {
            mTexCoordGens[index].function = GX_TG_SRTG;
            mTexCoordGens[index].source =
                texGenType == 3u ? GX_TG_COLOR1 : GX_TG_COLOR0;
        } else {
            mTexCoordGens[index].function =
                (texGenWord & (1u << 1u)) != 0u
                    ? GX_TG_MTX2x4
                    : GX_TG_MTX3x4;
        }
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

void GlobalState::RefreshViewportTransform(u32 firstAddress, u32 endAddress) {
    constexpr u32 viewportStart = 0x101A;
    constexpr u32 viewportEnd = viewportStart + 6;
    if (endAddress <= viewportStart || firstAddress >= viewportEnd) {
        return;
    }

    for (size_t index = 0; index < mViewportTransform.size(); ++index) {
        mViewportTransform[index] =
            WordToFloat(mXfMemory[viewportStart + index]);
    }
    mViewportTransformValid = true;

    const float halfWidth = mViewportTransform[0];
    const float negativeHalfHeight = mViewportTransform[1];
    mViewportState.left =
        mViewportTransform[3] - 342.0f - halfWidth;
    mViewportState.top =
        mViewportTransform[4] - 342.0f + negativeHalfHeight;
    mViewportState.width = halfWidth * 2.0f;
    mViewportState.height = negativeHalfHeight * -2.0f;
    if (mViewportState.width > 0.0f && mViewportState.height > 0.0f) {
        mViewportState.referenceWidth = std::max(
            mViewportState.referenceWidth,
            mViewportState.left + mViewportState.width);
        mViewportState.referenceHeight = std::max(
            mViewportState.referenceHeight,
            mViewportState.top + mViewportState.height);
        mViewportState.valid = true;
    }
}

void GlobalState::RefreshChannelState(u32 firstAddress, u32 endAddress) {
    constexpr u32 channelStateStart = 0x100a;
    constexpr u32 channelStateEnd = 0x1012;
    if (endAddress <= channelStateStart || firstAddress >= channelStateEnd) {
        return;
    }

    for (size_t channel = 0; channel < mChannels.size(); ++channel) {
        auto& state = mChannels[channel];
        state.ambientColor =
            DecodeXfColor(mXfMemory[0x100a + channel]);
        state.materialColor =
            DecodeXfColor(mXfMemory[0x100c + channel]);
        state.colorControl =
            DecodeChannelControl(mXfMemory[0x100e + channel]);
        state.alphaControl =
            DecodeChannelControl(mXfMemory[0x1010 + channel]);
    }
}

void GlobalState::RefreshLightState(u32 firstAddress, u32 endAddress) {
    constexpr u32 firstLightAddress = 0x600;
    constexpr u32 wordsPerLight = 0x10;
    for (size_t index = 0; index < mLights.size(); ++index) {
        const u32 lightStart =
            firstLightAddress + static_cast<u32>(index) * wordsPerLight;
        const u32 lightEnd = lightStart + wordsPerLight;
        if (endAddress <= lightStart || firstAddress >= lightEnd) {
            continue;
        }

        auto& light = mLights[index];
        light.color = DecodeXfColor(mXfMemory[lightStart + 3u]);
        for (size_t component = 0; component < 3; ++component) {
            light.cosineAttenuation[component] =
                WordToFloat(mXfMemory[lightStart + 4u + component]);
            light.distanceAttenuation[component] =
                WordToFloat(mXfMemory[lightStart + 7u + component]);
            light.position[component] =
                WordToFloat(mXfMemory[lightStart + 10u + component]);
            light.direction[component] =
                WordToFloat(mXfMemory[lightStart + 13u + component]);
        }
        light.valid = true;
    }
}

void InitGlobalState() {
    sGXGlobalState.Reset();
}

GlobalState& GetGlobalState() {
    return sGXGlobalState;
}
}
