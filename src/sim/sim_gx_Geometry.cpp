#include "simulator/sim_gx_Geometry.hpp"

#include <cmath>
#include <cstring>

#include <dolphin.h>

#include "dolphin/gx/GXEnum.h"
#include "simulator/sim_gx_GlRenderer.hpp"
#include "simulator/sim_gx_State.hpp"

namespace {

template <typename T>
static inline T ReadUnaligned(const u8* source) {
    T value;
    std::memcpy(&value, source, sizeof(value));
    return value;
}

static inline size_t ComponentSize(GXCompType type) {
    switch (type) {
        case GX_U8:
        case GX_S8:
            return 1;
        case GX_U16:
        case GX_S16:
            return 2;
        case GX_F32:
            return 4;
        default:
            return 0;
    }
}

void NoOpComponent(const u8 * source, GXCompCnt dummy, GXCompType type, u8 fraction, float * output) {

}

void DecodePositionComponent(const u8* source, GXCompCnt dummy, GXCompType type, u8 fraction, float * output) {
    switch (type) {
        case GX_U8:
            *output = std::ldexp(static_cast<float>(ReadUnaligned<u8>(source)), -fraction);
            break;
        case GX_S8:
            *output = std::ldexp(static_cast<float>(ReadUnaligned<s8>(source)), -fraction);
            break;
        case GX_U16:
            *output = std::ldexp(static_cast<float>(ReadUnaligned<u16>(source)), -fraction);
            break;
        case GX_S16:
            *output = std::ldexp(static_cast<float>(ReadUnaligned<s16>(source)), -fraction);
            break;
        case GX_F32:
            *output =  ReadUnaligned<f32>(source);
            break;
        default:
            *output = 0.0f;
            break;
    }
}

static inline void DecodeColor(const u8* source, GXCompCnt componentCount, GXCompType type, u8 dummyFrac,
                 float *output ) {
    u8 rgba[4] = {255, 255, 255, 255};

    switch (type) {
        case GX_RGB565: {
            const u16 packed = ReadUnaligned<u16>(source);
            rgba[0] = static_cast<u8>(((packed >> 11) & 0x1f) * 255 / 31);
            rgba[1] = static_cast<u8>(((packed >> 5) & 0x3f) * 255 / 63);
            rgba[2] = static_cast<u8>((packed & 0x1f) * 255 / 31);
            break;
        }
        case GX_RGB8:
        case GX_RGBX8:
            rgba[0] = source[0];
            rgba[1] = source[1];
            rgba[2] = source[2];
            break;
        case GX_RGBA4: {
            const u16 packed = ReadUnaligned<u16>(source);
            rgba[0] = static_cast<u8>(((packed >> 12) & 0x0f) * 17);
            rgba[1] = static_cast<u8>(((packed >> 8) & 0x0f) * 17);
            rgba[2] = static_cast<u8>(((packed >> 4) & 0x0f) * 17);
            rgba[3] = static_cast<u8>((packed & 0x0f) * 17);
            break;
        }
        case GX_RGBA6: {
            const u32 packed =
                (static_cast<u32>(source[0]) << 16) |
                (static_cast<u32>(source[1]) << 8) |
                static_cast<u32>(source[2]);
            rgba[0] = static_cast<u8>((((packed >> 18) & 0x3f) * 255 + 31) / 63);
            rgba[1] = static_cast<u8>((((packed >> 12) & 0x3f) * 255 + 31) / 63);
            rgba[2] = static_cast<u8>((((packed >> 6) & 0x3f) * 255 + 31) / 63);
            rgba[3] = static_cast<u8>(((packed & 0x3f) * 255 + 31) / 63);
            break;
        }
        case GX_RGBA8:
        default:
            rgba[0] = source[0];
            rgba[1] = source[1];
            rgba[2] = source[2];
            rgba[3] = componentCount == GX_CLR_RGBA ? source[3] : 255;
            break;
    }

    constexpr float byteScale = 1.0f / 255.0f;
    for (size_t i = 0; i < 4; ++i) {
        output[i] = static_cast<float>(rgba[i]) * byteScale;
    }
}

static inline bool ReadArrayIndex(const u8*& cursor, const u8* end, GXAttrType descriptor,
                    size_t& index) {

    switch(descriptor) {
        case GX_INDEX8:
            if (cursor + sizeof(u8) > end) {
                return false;
            }
            index = *cursor++;
            return true;
        case GX_INDEX16:
            if (cursor + sizeof(u16) > end) {
                return false;
            }
            index = ReadUnaligned<u16>(cursor);
            cursor += sizeof(u16);
            return true;
        default:
            return false;
    }
}

}

namespace SIM::GX {

GeometryProcessor::GeometryProcessor() {}

void GeometryProcessor::ProcessByteStream(std::vector<u8>& byteStream) {
    auto& gxState = GetGlobalState();
    //mRenderVerts.clear();

    const size_t bytesPerVertex = gxState.GetNumBytesPerVertex();
    if (bytesPerVertex == 0 || byteStream.empty() ||
        byteStream.size() % bytesPerVertex != 0) {
        return;
    }

    const size_t numVertices = byteStream.size() / bytesPerVertex;
    const auto& format = gxState.GetCurrentVertexFormat();
    const u8* cursor = byteStream.data();
    const u8* end = cursor + byteStream.size();
    if(numVertices > mRenderVertsSize) {
        // Reallocate mRenderVerts
        if(mRenderVerts) {
            delete mRenderVerts;
        }
        mRenderVerts = new RenderVertex[numVertices];
        mRenderVertsSize = numVertices;
    }

    // These cannot change during the loop
    struct {
        GXAttrType mDescriptor;
        size_t mComponentCount;
        size_t mComponentSize;
        VertexArray mVertexArray;
    } vtxInfo[GX_VA_MAX_ATTR] = {};


    auto ProcessAttribute = [&gxState, &vtxInfo, &format](GXAttr attr, size_t (*numComponentsFunc)(GXCompCnt)) mutable -> void {
        auto& curFormat = format.mAttributes[attr];
        vtxInfo[attr].mDescriptor = gxState.GetVertexDescriptor(attr);
        vtxInfo[attr].mComponentCount = numComponentsFunc(curFormat.mComponents);
        if(attr == GX_VA_CLR0 || attr == GX_VA_CLR1) {
             vtxInfo[attr].mComponentSize = gxState.GetDescriptorSize(GX_DIRECT, curFormat.mDataType, true);
        } else {
            vtxInfo[attr].mComponentSize = ComponentSize(curFormat.mDataType);
        }
        vtxInfo[attr].mVertexArray = gxState.GetVertexArray(attr);
    };

    ProcessAttribute(GX_VA_PNMTXIDX, gxState.GetNumMtxIdxComponents);
    for(int i=GX_VA_TEX0MTXIDX; i<=GX_VA_TEX7MTXIDX; i++) {
        ProcessAttribute(static_cast<GXAttr>(i), gxState.GetNumMtxIdxComponents);
    }

    ProcessAttribute(GX_VA_POS, gxState.GetNumPositionComponents);
    ProcessAttribute(GX_VA_NRM, gxState.GetNumNormalComponents);

    ProcessAttribute(GX_VA_CLR0, gxState.GetNumColorComponents);
    ProcessAttribute(GX_VA_CLR1, gxState.GetNumColorComponents);   
    for(int attribNum = GX_VA_TEX0; attribNum <= GX_VA_TEX7; attribNum++) {
        ProcessAttribute(static_cast<GXAttr>(attribNum), gxState.GetNumTexCoordComponents); 
    }

    ProcessAttribute(GX_VA_NBT, gxState.GetNumNBTComponents);

    auto BuildRenderVertexAttr = [&cursor, &end, &vtxInfo, &format](GXAttr attr, float * outputArray, void (*decodeComponentFunc)(const u8*, GXCompCnt, GXCompType, u8, float *)) mutable -> void {
        const auto& info = vtxInfo[attr];
        if(info.mDescriptor == GX_NONE) {
            return;
        }

        const u8* dataSource = nullptr;
        if (info.mDescriptor == GX_DIRECT) {
            const size_t directSize = info.mComponentCount * info.mComponentSize;
            if (cursor + directSize > end) {
                return;
            }
            dataSource = cursor;
            cursor += directSize;
        } else {
            size_t arrayIndex;
            if (!ReadArrayIndex(cursor, end, info.mDescriptor, arrayIndex)) {
                return;
            }
            
            dataSource = static_cast<const u8*>(info.mVertexArray.mArrayPtr) +
                             arrayIndex * static_cast<size_t>(info.mVertexArray.mStride);
        }

        for (size_t component = 0; component < info.mComponentCount; ++component) {
                decodeComponentFunc(
                dataSource + component * info.mComponentSize,
                format.mAttributes[attr].mComponents,
                format.mAttributes[attr].mDataType,
                format.mAttributes[attr].mFraction,
                &outputArray[component]);
        }
    };

    for (size_t vertexIndex = 0; vertexIndex < numVertices; ++vertexIndex) {
        RenderVertex& output = mRenderVerts[vertexIndex];

        BuildRenderVertexAttr(GX_VA_PNMTXIDX, nullptr, NoOpComponent);
        for(int i=GX_VA_TEX0MTXIDX; i <=GX_VA_TEX7MTXIDX; i++) {
            BuildRenderVertexAttr(static_cast<GXAttr>(i), nullptr, NoOpComponent);
        }

        BuildRenderVertexAttr(GX_VA_POS, output.position.coords, DecodePositionComponent);
        BuildRenderVertexAttr(GX_VA_NRM, output.normal.coords, DecodePositionComponent);

        BuildRenderVertexAttr(GX_VA_CLR0, output.color0.values, DecodeColor);
        BuildRenderVertexAttr(GX_VA_CLR1, nullptr, NoOpComponent);

        for(int attribNum = GX_VA_TEX0; attribNum <= GX_VA_TEX7; attribNum++) {
            BuildRenderVertexAttr(static_cast<GXAttr>(attribNum), output.texCoords[attribNum - GX_VA_TEX0].coords, DecodePositionComponent);
        }

        BuildRenderVertexAttr(GX_VA_NBT, nullptr, NoOpComponent);

    }

    GetGlRenderer().Draw(mRenderVerts, numVertices, gxState.GetCurrentPrimitive());
}
}
