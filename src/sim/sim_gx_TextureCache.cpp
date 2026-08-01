#include <simulator/sim_gx_GlRenderer.hpp>

#include <cstring>

#include <simulator/sim_gx_State.hpp>

namespace SIM::GX {

namespace {

// GX texture image dimensions are encoded as 10-bit (size minus one) fields.
// Reject host descriptors that cannot exist on the original hardware before
// calculating source ranges or allocating an RGBA decode buffer.
constexpr size_t kMaxTextureDimension = 1024u;

struct TextureBlockLayout {
    size_t width = 0;
    size_t height = 0;
    size_t byteSize = 0;
};

bool GetTextureBlockLayout(
    GXTexFmt format,
    TextureBlockLayout& layout) {
    switch (format) {
        case GX_TF_I4:
        case static_cast<GXTexFmt>(GX_TF_C4):
        case GX_TF_CMPR:
            layout = {8u, 8u, 32u};
            return true;
        case GX_TF_I8:
        case GX_TF_IA4:
        case static_cast<GXTexFmt>(GX_TF_C8):
        case GX_TF_Z8:
            layout = {8u, 4u, 32u};
            return true;
        case GX_TF_IA8:
        case GX_TF_RGB565:
        case GX_TF_RGB5A3:
        case static_cast<GXTexFmt>(GX_TF_C14X2):
        case GX_TF_Z16:
            layout = {4u, 4u, 32u};
            return true;
        case GX_TF_RGBA8:
        case GX_TF_Z24X8:
            layout = {4u, 4u, 64u};
            return true;
        default:
            return false;
    }
}

size_t GetMipLevelByteSize(
    u16 width,
    u16 height,
    const TextureBlockLayout& block) {
    const size_t blockColumns =
        (static_cast<size_t>(width) + block.width - 1u) /
        block.width;
    const size_t blockRows =
        (static_cast<size_t>(height) + block.height - 1u) /
        block.height;
    return blockColumns * blockRows * block.byteSize;
}

}

size_t GetTextureMipLevelCount(const TextureState& texture) {
    if (texture.width == 0u || texture.height == 0u ||
        texture.width > kMaxTextureDimension ||
        texture.height > kMaxTextureDimension) {
        return 0u;
    }

    TextureBlockLayout block;
    if (!GetTextureBlockLayout(texture.format, block)) {
        return 0u;
    }
    if (!texture.mipmap) {
        return 1u;
    }

    size_t physicalMaximumLevel = 0u;
    u16 width = texture.width;
    u16 height = texture.height;
    while (width > 1u || height > 1u) {
        width = width > 1u ? static_cast<u16>(width >> 1u) : 1u;
        height = height > 1u ? static_cast<u16>(height >> 1u) : 1u;
        ++physicalMaximumLevel;
    }

    float maximumLod = texture.maxLod;
    if (!(maximumLod > 0.0f)) {
        maximumLod = 0.0f;
    } else if (maximumLod > 10.0f) {
        maximumLod = 10.0f;
    }
    size_t requestedMaximumLevel = static_cast<size_t>(maximumLod);
    if (static_cast<float>(requestedMaximumLevel) < maximumLod) {
        ++requestedMaximumLevel;
    }
    if (requestedMaximumLevel > physicalMaximumLevel) {
        requestedMaximumLevel = physicalMaximumLevel;
    }
    return requestedMaximumLevel + 1u;
}

bool GetTextureMipLevelLayout(
    const TextureState& texture,
    size_t level,
    TextureMipLevelLayout& layout) {
    const size_t levelCount = GetTextureMipLevelCount(texture);
    TextureBlockLayout block;
    if (level >= levelCount ||
        !GetTextureBlockLayout(texture.format, block)) {
        layout = {};
        return false;
    }

    size_t offset = 0u;
    u16 width = texture.width;
    u16 height = texture.height;
    for (size_t currentLevel = 0u;
         currentLevel <= level;
         ++currentLevel) {
        const size_t byteSize =
            GetMipLevelByteSize(width, height, block);
        if (currentLevel == level) {
            layout.offset = offset;
            layout.byteSize = byteSize;
            layout.width = width;
            layout.height = height;
            return true;
        }
        offset += byteSize;
        width = width > 1u ? static_cast<u16>(width >> 1u) : 1u;
        height = height > 1u ? static_cast<u16>(height >> 1u) : 1u;
    }
    layout = {};
    return false;
}

size_t GetTextureSourceByteSize(const TextureState& texture) {
    const size_t levelCount = GetTextureMipLevelCount(texture);
    TextureMipLevelLayout finalLevel;
    if (levelCount == 0u ||
        !GetTextureMipLevelLayout(texture, levelCount - 1u, finalLevel)) {
        return 0u;
    }
    return finalLevel.offset + finalLevel.byteSize;
}

bool CopyCanonicalTextureBytes(
    const TextureState& texture,
    std::vector<u8>& canonicalBytes) {
    const size_t byteSize = GetTextureSourceByteSize(texture);
    if (texture.data == nullptr || byteSize == 0u) {
        canonicalBytes.clear();
        return false;
    }

    canonicalBytes.resize(byteSize);
    if (texture.sourceEncoding ==
        TextureState::SourceEncoding::NativeU16) {
        const auto* sourceWords =
            static_cast<const u16*>(texture.data);
        for (size_t word = 0; word < byteSize / 2u; ++word) {
            const u16 value = sourceWords[word];
            canonicalBytes[word * 2u] =
                static_cast<u8>(value >> 8u);
            canonicalBytes[word * 2u + 1u] =
                static_cast<u8>(value);
        }
        if ((byteSize & 1u) != 0u) {
            canonicalBytes.back() =
                static_cast<const u8*>(texture.data)[byteSize - 1u];
        }
    } else {
        std::memcpy(canonicalBytes.data(), texture.data, byteSize);
    }
    return true;
}

DecodedTlutColor DecodeTlutEntry(
    GXTlutFmt format,
    const u8* canonicalBigEndianBytes) {
    DecodedTlutColor color;
    if (canonicalBigEndianBytes == nullptr) {
        return color;
    }

    const u16 packed = static_cast<u16>(
        (static_cast<u16>(canonicalBigEndianBytes[0]) << 8u) |
        static_cast<u16>(canonicalBigEndianBytes[1]));
    if (format == GX_TL_RGB565) {
        color.red = static_cast<u8>(
            ((packed >> 11u) & 0x1fu) * 255u / 31u);
        color.green = static_cast<u8>(
            ((packed >> 5u) & 0x3fu) * 255u / 63u);
        color.blue = static_cast<u8>(
            (packed & 0x1fu) * 255u / 31u);
        color.alpha = 255u;
    } else if (format == GX_TL_RGB5A3) {
        if ((packed & 0x8000u) != 0u) {
            color.red = static_cast<u8>(
                ((packed >> 10u) & 0x1fu) * 255u / 31u);
            color.green = static_cast<u8>(
                ((packed >> 5u) & 0x1fu) * 255u / 31u);
            color.blue = static_cast<u8>(
                (packed & 0x1fu) * 255u / 31u);
            color.alpha = 255u;
        } else {
            color.red = static_cast<u8>(
                ((packed >> 8u) & 0x0fu) * 17u);
            color.green = static_cast<u8>(
                ((packed >> 4u) & 0x0fu) * 17u);
            color.blue = static_cast<u8>(
                (packed & 0x0fu) * 17u);
            color.alpha = static_cast<u8>(
                ((packed >> 12u) & 0x07u) * 255u / 7u);
        }
    } else {
        // GX_TL_IA8 is serialized as alpha followed by intensity in
        // GameCube memory. `packed` is the canonical big-endian word, so the
        // high byte is alpha and the low byte is intensity.
        const u8 intensity = static_cast<u8>(packed & 0xffu);
        color.red = intensity;
        color.green = intensity;
        color.blue = intensity;
        color.alpha = static_cast<u8>(packed >> 8u);
    }
    return color;
}

bool TextureContentSnapshot::Matches(
    const TextureState& texture,
    const TlutState* tlut) const {
    const size_t textureByteSize = GetTextureSourceByteSize(texture);
    if (!mValid || texture.data == nullptr || textureByteSize == 0u ||
        mWidth != texture.width || mHeight != texture.height ||
        mFormat != texture.format ||
        mTextureBytes.size() != textureByteSize) {
        return false;
    }

    std::vector<u8> currentTextureBytes;
    if (!CopyCanonicalTextureBytes(texture, currentTextureBytes) ||
        currentTextureBytes != mTextureBytes) {
        return false;
    }

    const bool usesTlut =
        texture.format == GX_TF_C4 ||
        texture.format == GX_TF_C8 ||
        texture.format == GX_TF_C14X2;
    if (!usesTlut) {
        return mTlutBytes.empty();
    }

    const GXTlutFmt tlutFormat =
        tlut != nullptr ? tlut->format : GX_TL_IA8;
    const u16 tlutEntries =
        tlut != nullptr ? tlut->entries : 0u;
    const size_t tlutByteSize =
        tlut != nullptr && tlut->CanonicalData() != nullptr
            ? static_cast<size_t>(tlutEntries) * 2u
            : 0u;
    return
        mTlutFormat == tlutFormat &&
        mTlutEntries == tlutEntries &&
        mTlutBytes.size() == tlutByteSize &&
        (tlutByteSize == 0u ||
         std::memcmp(
             mTlutBytes.data(),
             tlut->CanonicalData(),
             tlutByteSize) == 0);
}

void TextureContentSnapshot::Capture(
    const TextureState& texture,
    const TlutState* tlut) {
    mWidth = texture.width;
    mHeight = texture.height;
    mFormat = texture.format;
    mValid = CopyCanonicalTextureBytes(texture, mTextureBytes);

    const bool usesTlut =
        texture.format == GX_TF_C4 ||
        texture.format == GX_TF_C8 ||
        texture.format == GX_TF_C14X2;
    mTlutFormat =
        usesTlut && tlut != nullptr ? tlut->format : GX_TL_IA8;
    mTlutEntries =
        usesTlut && tlut != nullptr ? tlut->entries : 0u;
    if (usesTlut && tlut != nullptr && tlut->CanonicalData() != nullptr) {
        const auto* bytes =
            static_cast<const u8*>(tlut->CanonicalData());
        const size_t byteSize = static_cast<size_t>(tlut->entries) * 2u;
        mTlutBytes.assign(bytes, bytes + byteSize);
    } else {
        mTlutBytes.clear();
    }
}

}
