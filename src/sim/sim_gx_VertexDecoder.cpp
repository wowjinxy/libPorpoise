#include "simulator/sim_gx_Geometry.hpp"

#include <cmath>
#include <cstring>

#include "simulator/sim_gx_State.hpp"

namespace {

template <typename T>
T ReadUnaligned(const u8* source) {
    T value;
    std::memcpy(&value, source, sizeof(value));
    return value;
}

u16 ReadU16(const u8* source, bool bigEndian) {
    if (bigEndian) {
        return static_cast<u16>(
            (static_cast<u16>(source[0]) << 8) |
            static_cast<u16>(source[1]));
    }
    return ReadUnaligned<u16>(source);
}

u32 ReadU32(const u8* source, bool bigEndian) {
    if (bigEndian) {
        return
            (static_cast<u32>(source[0]) << 24) |
            (static_cast<u32>(source[1]) << 16) |
            (static_cast<u32>(source[2]) << 8) |
            static_cast<u32>(source[3]);
    }
    return ReadUnaligned<u32>(source);
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

float DecodeComponent(
    const u8* source,
    GXCompType type,
    u8 fraction,
    bool bigEndian) {
    switch (type) {
        case GX_U8:
            return std::ldexp(static_cast<float>(*source), -fraction);
        case GX_S8:
            return std::ldexp(
                static_cast<float>(static_cast<s8>(*source)), -fraction);
        case GX_U16:
            return std::ldexp(
                static_cast<float>(ReadU16(source, bigEndian)), -fraction);
        case GX_S16:
            return std::ldexp(
                static_cast<float>(
                    static_cast<s16>(ReadU16(source, bigEndian))),
                -fraction);
        case GX_F32: {
            const u32 bits = ReadU32(source, bigEndian);
            float value;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }
        default:
            return 0.0f;
    }
}

bool NormalFraction(GXCompType type, u8& fraction) {
    switch (type) {
        case GX_S8:
            fraction = 6;
            return true;
        case GX_S16:
            fraction = 14;
            return true;
        case GX_F32:
            fraction = 0;
            return true;
        default:
            return false;
    }
}

void DecodeColor(
    const u8* source,
    GXCompCnt componentCount,
    GXCompType type,
    float* output,
    bool bigEndian) {
    u8 rgba[4] = {255, 255, 255, 255};

    switch (type) {
        case GX_RGB565: {
            const u16 packed = ReadU16(source, bigEndian);
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
            const u16 packed = ReadU16(source, bigEndian);
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
            rgba[0] = source[0];
            rgba[1] = source[1];
            rgba[2] = source[2];
            rgba[3] = componentCount == GX_CLR_RGBA ? source[3] : 255;
            break;
        default:
            return;
    }

    constexpr float byteScale = 1.0f / 255.0f;
    for (size_t component = 0; component < 4; ++component) {
        output[component] = static_cast<float>(rgba[component]) * byteScale;
    }
}

bool ReadIndex(
    const u8*& cursor,
    const u8* end,
    GXAttrType descriptor,
    bool bigEndian,
    size_t& index,
    bool& disabled) {
    size_t reservedIndex = 0;
    if (descriptor == GX_INDEX8) {
        if (cursor == end) {
            return false;
        }
        index = *cursor++;
        reservedIndex = 0xff;
    } else if (descriptor == GX_INDEX16) {
        if (static_cast<size_t>(end - cursor) < sizeof(u16)) {
            return false;
        }
        index = ReadU16(cursor, bigEndian);
        cursor += sizeof(u16);
        reservedIndex = 0xffff;
    } else {
        return false;
    }

    disabled = index == reservedIndex;
    return true;
}

bool ResolveAttributeSource(
    const SIM::GX::GlobalState& state,
    GXAttr attribute,
    GXAttrType descriptor,
    size_t directSize,
    const u8*& cursor,
    const u8* end,
    bool streamBigEndian,
    const u8*& source,
    bool& sourceBigEndian,
    bool& disabled) {
    source = nullptr;
    sourceBigEndian = false;
    disabled = false;

    if (descriptor == GX_DIRECT) {
        if (static_cast<size_t>(end - cursor) < directSize) {
            return false;
        }
        source = cursor;
        sourceBigEndian = streamBigEndian;
        cursor += directSize;
        return true;
    }

    size_t index = 0;
    if (!ReadIndex(
            cursor,
            end,
            descriptor,
            streamBigEndian,
            index,
            disabled)) {
        return false;
    }
    if (disabled) {
        return true;
    }

    const auto& array = state.GetVertexArray(attribute);
    if (array.mArrayPtr == nullptr || array.mStride < 0) {
        return false;
    }
    source = static_cast<const u8*>(array.mArrayPtr) +
             index * static_cast<size_t>(array.mStride);
    return true;
}

bool DecodeVector(
    const u8* source,
    size_t componentCount,
    size_t componentSize,
    GXCompType type,
    u8 fraction,
    bool bigEndian,
    float* output) {
    if (source == nullptr || componentSize == 0) {
        return false;
    }
    for (size_t component = 0; component < componentCount; ++component) {
        output[component] = DecodeComponent(
            source + component * componentSize,
            type,
            fraction,
            bigEndian);
    }
    return true;
}

}

namespace SIM::GX {

bool DecodeVertexStream(
    const GlobalState& state,
    std::span<const u8> byteStream,
    bool bigEndian,
    std::vector<RenderVertex>& output) {
    output.clear();

    const size_t bytesPerVertex = state.GetNumBytesPerVertex();
    if (bytesPerVertex == 0 || byteStream.empty() ||
        byteStream.size() % bytesPerVertex != 0) {
        return false;
    }

    const size_t numVertices = byteStream.size() / bytesPerVertex;
    const auto& format = state.GetCurrentVertexFormat();
    const u8* cursor = byteStream.data();
    const u8* end = cursor + byteStream.size();
    output.reserve(numVertices);

    for (size_t vertexIndex = 0; vertexIndex < numVertices; ++vertexIndex) {
        RenderVertex vertex;
        bool vertexDisabled = false;

        for (u32 attr = GX_VA_PNMTXIDX; attr <= GX_VA_TEX7MTXIDX; ++attr) {
            if (state.GetVertexDescriptor(static_cast<GXAttr>(attr)) == GX_NONE) {
                continue;
            }
            if (cursor == end) {
                output.clear();
                return false;
            }
            const u8 matrixIndex = static_cast<u8>(*cursor++ & 0x3f);
            if (attr == GX_VA_PNMTXIDX) {
                vertex.positionMatrixIndex = matrixIndex;
            } else {
                vertex.textureMatrixIndices[attr - GX_VA_TEX0MTXIDX] =
                    matrixIndex;
            }
        }

        const GXAttrType positionDescriptor =
            state.GetVertexDescriptor(GX_VA_POS);
        if (positionDescriptor != GX_NONE) {
            const auto& attributes = format.mAttributes[GX_VA_POS];
            const size_t componentCount =
                state.GetNumPositionComponents(attributes.mComponents);
            const size_t componentSize = ComponentSize(attributes.mDataType);
            if (componentSize == 0) {
                output.clear();
                return false;
            }

            const u8* source = nullptr;
            bool sourceBigEndian = false;
            bool disabled = false;
            if (!ResolveAttributeSource(
                    state,
                    GX_VA_POS,
                    positionDescriptor,
                    componentCount * componentSize,
                    cursor,
                    end,
                    bigEndian,
                    source,
                    sourceBigEndian,
                    disabled)) {
                output.clear();
                return false;
            }
            vertexDisabled = vertexDisabled || disabled;
            if (!disabled &&
                !DecodeVector(
                    source,
                    componentCount,
                    componentSize,
                    attributes.mDataType,
                    attributes.mFraction,
                    sourceBigEndian,
                    vertex.position.Data())) {
                output.clear();
                return false;
            }
        }

        const GXAttrType normalDescriptor =
            state.GetVertexDescriptor(GX_VA_NRM);
        if (normalDescriptor != GX_NONE) {
            const auto& attributes = format.mAttributes[GX_VA_NRM];
            const size_t componentSize = ComponentSize(attributes.mDataType);
            u8 fraction = 0;
            if (componentSize == 0 ||
                !NormalFraction(attributes.mDataType, fraction)) {
                output.clear();
                return false;
            }

            RenderVector3* vectors[3] = {
                &vertex.normal,
                &vertex.binormal,
                &vertex.tangent,
            };
            const bool nbt = attributes.mComponents != GX_NRM_XYZ;
            const bool independentlyIndexed =
                attributes.mComponents == GX_NRM_NBT3 &&
                (normalDescriptor == GX_INDEX8 ||
                 normalDescriptor == GX_INDEX16);

            if (independentlyIndexed) {
                for (size_t group = 0; group < 3; ++group) {
                    const u8* source = nullptr;
                    bool sourceBigEndian = false;
                    bool disabled = false;
                    if (!ResolveAttributeSource(
                            state,
                            GX_VA_NRM,
                            normalDescriptor,
                            3 * componentSize,
                            cursor,
                            end,
                            bigEndian,
                            source,
                            sourceBigEndian,
                            disabled)) {
                        output.clear();
                        return false;
                    }
                    vertexDisabled = vertexDisabled || disabled;
                    if (!disabled) {
                        source += group * 3 * componentSize;
                        if (!DecodeVector(
                                source,
                                3,
                                componentSize,
                                attributes.mDataType,
                                fraction,
                                sourceBigEndian,
                                vectors[group]->Data())) {
                            output.clear();
                            return false;
                        }
                    }
                }
            } else {
                const size_t vectorCount = nbt ? 3 : 1;
                const u8* source = nullptr;
                bool sourceBigEndian = false;
                bool disabled = false;
                if (!ResolveAttributeSource(
                        state,
                        GX_VA_NRM,
                        normalDescriptor,
                        vectorCount * 3 * componentSize,
                        cursor,
                        end,
                        bigEndian,
                        source,
                        sourceBigEndian,
                        disabled)) {
                    output.clear();
                    return false;
                }
                vertexDisabled = vertexDisabled || disabled;
                if (!disabled) {
                    for (size_t group = 0; group < vectorCount; ++group) {
                        if (!DecodeVector(
                                source + group * 3 * componentSize,
                                3,
                                componentSize,
                                attributes.mDataType,
                                fraction,
                                sourceBigEndian,
                                vectors[group]->Data())) {
                            output.clear();
                            return false;
                        }
                    }
                }
            }
        }

        for (u32 attr = GX_VA_CLR0; attr <= GX_VA_CLR1; ++attr) {
            const GXAttr attribute = static_cast<GXAttr>(attr);
            const GXAttrType descriptor = state.GetVertexDescriptor(attribute);
            if (descriptor == GX_NONE) {
                continue;
            }

            const auto& attributes = format.mAttributes[attribute];
            const size_t colorSize =
                state.GetDescriptorSize(GX_DIRECT, attributes.mDataType, true);
            if (colorSize == 0) {
                output.clear();
                return false;
            }

            const u8* source = nullptr;
            bool sourceBigEndian = false;
            bool disabled = false;
            if (!ResolveAttributeSource(
                    state,
                    attribute,
                    descriptor,
                    colorSize,
                    cursor,
                    end,
                    bigEndian,
                    source,
                    sourceBigEndian,
                    disabled)) {
                output.clear();
                return false;
            }
            vertexDisabled = vertexDisabled || disabled;
            if (!disabled) {
                RenderColor& color =
                    attribute == GX_VA_CLR0 ? vertex.color0 : vertex.color1;
                DecodeColor(
                    source,
                    attributes.mComponents,
                    attributes.mDataType,
                    color.Data(),
                    sourceBigEndian);
            }
        }

        for (u32 attr = GX_VA_TEX0; attr <= GX_VA_TEX7; ++attr) {
            const GXAttr attribute = static_cast<GXAttr>(attr);
            const GXAttrType descriptor = state.GetVertexDescriptor(attribute);
            if (descriptor == GX_NONE) {
                continue;
            }

            const auto& attributes = format.mAttributes[attribute];
            const size_t componentCount =
                attributes.mComponents == GX_TEX_ST ? 2 : 1;
            const size_t componentSize = ComponentSize(attributes.mDataType);
            if (componentSize == 0) {
                output.clear();
                return false;
            }

            const u8* source = nullptr;
            bool sourceBigEndian = false;
            bool disabled = false;
            if (!ResolveAttributeSource(
                    state,
                    attribute,
                    descriptor,
                    componentCount * componentSize,
                    cursor,
                    end,
                    bigEndian,
                    source,
                    sourceBigEndian,
                    disabled)) {
                output.clear();
                return false;
            }
            vertexDisabled = vertexDisabled || disabled;
            if (!disabled &&
                !DecodeVector(
                    source,
                    componentCount,
                    componentSize,
                    attributes.mDataType,
                    attributes.mFraction,
                    sourceBigEndian,
                    vertex.texCoords[attr - GX_VA_TEX0].Data())) {
                output.clear();
                return false;
            }
        }

        if (!vertexDisabled) {
            output.push_back(vertex);
        }
    }

    if (cursor != end) {
        output.clear();
        return false;
    }
    return true;
}

}
