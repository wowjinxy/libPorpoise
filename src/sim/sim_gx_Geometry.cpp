#include "simulator/sim_gx_Geometry.hpp"

#include <cmath>
#include <cstring>

#include <dolphin.h>

#include "simulator/sim_gx_GlRenderer.hpp"
#include "simulator/sim_gx_State.hpp"

namespace {

template <typename T>
T ReadUnaligned(const u8* source) {
    T value;
    std::memcpy(&value, source, sizeof(value));
    return value;
}

size_t ComponentSize(GXCompType type) {
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

float DecodePositionComponent(const u8* source, GXCompType type, u8 fraction) {
    switch (type) {
        case GX_U8:
            return std::ldexp(static_cast<float>(ReadUnaligned<u8>(source)), -fraction);
        case GX_S8:
            return std::ldexp(static_cast<float>(ReadUnaligned<s8>(source)), -fraction);
        case GX_U16:
            return std::ldexp(static_cast<float>(ReadUnaligned<u16>(source)), -fraction);
        case GX_S16:
            return std::ldexp(static_cast<float>(ReadUnaligned<s16>(source)), -fraction);
        case GX_F32:
            return ReadUnaligned<f32>(source);
        default:
            return 0.0f;
    }
}

void DecodeColor(const u8* source, GXCompCnt componentCount, GXCompType type,
                 float (&output)[4]) {
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

bool ReadArrayIndex(const u8*& cursor, const u8* end, GXAttrType descriptor,
                    size_t& index) {
    if (descriptor == GX_INDEX8) {
        if (cursor + sizeof(u8) > end) {
            return false;
        }
        index = *cursor++;
        return true;
    }

    if (descriptor == GX_INDEX16) {
        if (cursor + sizeof(u16) > end) {
            return false;
        }
        index = ReadUnaligned<u16>(cursor);
        cursor += sizeof(u16);
        return true;
    }

    return false;
}

}

namespace SIM::GX {

GeometryProcessor::GeometryProcessor() {}

void GeometryProcessor::ProcessByteStream(std::vector<u8>& byteStream) {
    auto& gxState = GetGlobalState();
    mRenderVerts.clear();

    const size_t bytesPerVertex = gxState.GetNumBytesPerVertex();
    if (bytesPerVertex == 0 || byteStream.empty() ||
        byteStream.size() % bytesPerVertex != 0) {
        return;
    }

    const size_t numVertices = byteStream.size() / bytesPerVertex;
    const auto& format = gxState.GetCurrentVertexFormat();
    const u8* cursor = byteStream.data();
    const u8* end = cursor + byteStream.size();
    mRenderVerts.reserve(numVertices);

    for (size_t vertexIndex = 0; vertexIndex < numVertices; ++vertexIndex) {
        RenderVertex output = {};
        output.color0.r = 1.0f;
        output.color0.g = 1.0f;
        output.color0.b = 1.0f;
        output.color0.a = 1.0f;

        const GXAttrType positionDescriptor = gxState.GetVertexDescriptor(GX_VA_POS);
        if (positionDescriptor != GX_NONE) {
            const auto& positionFormat = format.mAttributes[GX_VA_POS];
            const size_t componentCount =
                gxState.GetNumPositionComponents(positionFormat.mComponents);
            const size_t componentSize = ComponentSize(positionFormat.mDataType);
            if (componentSize == 0) {
                OSReport("SIM::GX: unsupported position component type\n");
                return;
            }

            const u8* positionSource = nullptr;
            if (positionDescriptor == GX_DIRECT) {
                const size_t directSize = componentCount * componentSize;
                if (cursor + directSize > end) {
                    return;
                }
                positionSource = cursor;
                cursor += directSize;
            } else {
                size_t arrayIndex;
                if (!ReadArrayIndex(cursor, end, positionDescriptor, arrayIndex)) {
                    return;
                }
                const auto& array = gxState.GetVertexArray(GX_VA_POS);
                if (array.mArrayPtr == nullptr || array.mStride < 0) {
                    OSReport("SIM::GX: position array is not configured\n");
                    return;
                }
                positionSource = static_cast<const u8*>(array.mArrayPtr) +
                                 arrayIndex * static_cast<size_t>(array.mStride);
            }

            for (size_t component = 0; component < componentCount; ++component) {
                output.position.coords[component] = DecodePositionComponent(
                    positionSource + component * componentSize,
                    positionFormat.mDataType,
                    positionFormat.mFraction);
            }
        }

        const GXAttrType colorDescriptor = gxState.GetVertexDescriptor(GX_VA_CLR0);
        if (colorDescriptor != GX_NONE) {
            const auto& colorFormat = format.mAttributes[GX_VA_CLR0];
            const size_t colorSize =
                gxState.GetDescriptorSize(GX_DIRECT, colorFormat.mDataType, true);
            if (colorSize == 0) {
                OSReport("SIM::GX: unsupported color format\n");
                return;
            }

            const u8* colorSource = nullptr;
            if (colorDescriptor == GX_DIRECT) {
                if (cursor + colorSize > end) {
                    return;
                }
                colorSource = cursor;
                cursor += colorSize;
            } else {
                size_t arrayIndex;
                if (!ReadArrayIndex(cursor, end, colorDescriptor, arrayIndex)) {
                    return;
                }
                const auto& array = gxState.GetVertexArray(GX_VA_CLR0);
                if (array.mArrayPtr == nullptr || array.mStride < 0) {
                    OSReport("SIM::GX: color array is not configured\n");
                    return;
                }
                colorSource = static_cast<const u8*>(array.mArrayPtr) +
                              arrayIndex * static_cast<size_t>(array.mStride);
            }

            DecodeColor(
                colorSource,
                colorFormat.mComponents,
                colorFormat.mDataType,
                output.color0.values);
        }

        mRenderVerts.push_back(output);
    }

    GetGlRenderer().Draw(mRenderVerts, gxState.GetCurrentPrimitive());
}
}
