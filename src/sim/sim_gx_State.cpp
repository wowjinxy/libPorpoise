#include <simulator/sim_gx_State.hpp>

static SIM::GX::GlobalState sGXGlobalState ={};

namespace SIM::GX {

GlobalState::GlobalState() {}

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
    // Get bytes from position
    size_t posComponents = 1;
    if(mVertexDescriptors[GX_VA_POS] == GX_DIRECT) {
        posComponents = format.mAttributes[GX_VA_POS].mComponents == GX_POS_XYZ ? 3 : 2;
    }

    totalBytes += posComponents * GetDescriptorSize(mVertexDescriptors[GX_VA_POS], format.mAttributes[GX_VA_POS].mDataType);

    // Get bytes from color0
    totalBytes += GetDescriptorSize(mVertexDescriptors[GX_VA_CLR0], format.mAttributes[GX_VA_CLR0].mDataType, true);

    return totalBytes;
}

size_t GlobalState::GetNumPositionComponents(GXCompCnt compType) {
    switch(compType) {
        case GX_POS_XY:
            return 2;
        default:
        case GX_POS_XYZ:
            return 3;
    }
}



const VertexArray& GlobalState::GetVertexArray(GXAttr attr) {
    return mVertexArrays[attr];
}

GXAttrType GlobalState::GetVertexDescriptor(GXAttr attr) {
    return mVertexDescriptors[attr];
}

const VertexFormat& GlobalState::GetCurrentVertexFormat() {
    return mVertexFormats[mCurrentVertexFormat];
}

const VertexFormat& GlobalState::GetVertexFormat(GXVtxFmt formatIdx) {
    return mVertexFormats[formatIdx];
}

void GlobalState::SetCurrentPrimitive(GXPrimitive primitive) {
    mCurrentPrimitive = primitive;
}

void GlobalState::SetCurrentVertexFormat(GXVtxFmt format) {
    mCurrentVertexFormat = format;
}

void GlobalState::SetVertexArray(GXAttr attr, VertexArray array) {
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



void InitGlobalState() {

}

GlobalState& GetGlobalState() {
    return sGXGlobalState;
}
}