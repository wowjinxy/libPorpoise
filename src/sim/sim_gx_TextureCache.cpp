#include <simulator/sim_gx_GlRenderer.hpp>

#include <bit>
#include <cstring>
#include <functional>
#include <utility>

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

bool MatchesCanonicalNativeU16Bytes(
    const void* source,
    const std::vector<u8>& canonicalBytes) {
    const size_t byteSize = canonicalBytes.size();
    if (source == nullptr || byteSize == 0u ||
        (byteSize & 1u) != 0u) {
        return false;
    }

    if constexpr (std::endian::native == std::endian::big) {
        return std::memcmp(source, canonicalBytes.data(), byteSize) == 0;
    }

    const auto* sourceBytes = static_cast<const u8*>(source);
    size_t byteOffset = 0u;
    if constexpr (std::endian::native == std::endian::little) {
        constexpr u64 lowBytes = 0x00ff00ff00ff00ffull;
        constexpr u64 highBytes = 0xff00ff00ff00ff00ull;
        for (;
             byteOffset + sizeof(u64) <= byteSize;
             byteOffset += sizeof(u64)) {
            u64 nativeWords = 0u;
            u64 canonicalWords = 0u;
            std::memcpy(
                &nativeWords,
                sourceBytes + byteOffset,
                sizeof(nativeWords));
            std::memcpy(
                &canonicalWords,
                canonicalBytes.data() + byteOffset,
                sizeof(canonicalWords));
            const u64 swappedWords =
                ((nativeWords & lowBytes) << 8u) |
                ((nativeWords & highBytes) >> 8u);
            if (swappedWords != canonicalWords) {
                return false;
            }
        }
    }

    for (;
         byteOffset < byteSize;
         byteOffset += sizeof(u16)) {
        u16 value = 0u;
        std::memcpy(
            &value,
            sourceBytes + byteOffset,
            sizeof(value));
        if (canonicalBytes[byteOffset] !=
                static_cast<u8>(value >> 8u) ||
            canonicalBytes[byteOffset + 1u] !=
                static_cast<u8>(value)) {
            return false;
        }
    }
    return true;
}

bool CopyCanonicalNativeU16Bytes(
    const void* source,
    size_t byteSize,
    std::vector<u8>& canonicalBytes) {
    if (source == nullptr || byteSize == 0u ||
        (byteSize & 1u) != 0u) {
        canonicalBytes.clear();
        return false;
    }

    canonicalBytes.resize(byteSize);
    const auto* sourceBytes = static_cast<const u8*>(source);
    if constexpr (std::endian::native == std::endian::big) {
        std::memcpy(canonicalBytes.data(), source, byteSize);
        return true;
    }

    size_t byteOffset = 0u;
    if constexpr (std::endian::native == std::endian::little) {
        constexpr u64 lowBytes = 0x00ff00ff00ff00ffull;
        constexpr u64 highBytes = 0xff00ff00ff00ff00ull;
        for (;
             byteOffset + sizeof(u64) <= byteSize;
             byteOffset += sizeof(u64)) {
            u64 nativeWords = 0u;
            std::memcpy(
                &nativeWords,
                sourceBytes + byteOffset,
                sizeof(nativeWords));
            const u64 swappedWords =
                ((nativeWords & lowBytes) << 8u) |
                ((nativeWords & highBytes) >> 8u);
            std::memcpy(
                canonicalBytes.data() + byteOffset,
                &swappedWords,
                sizeof(swappedWords));
        }
    }

    for (;
         byteOffset < byteSize;
         byteOffset += sizeof(u16)) {
        u16 value = 0u;
        std::memcpy(
            &value,
            sourceBytes + byteOffset,
            sizeof(value));
        canonicalBytes[byteOffset] = static_cast<u8>(value >> 8u);
        canonicalBytes[byteOffset + 1u] = static_cast<u8>(value);
    }
    return true;
}

bool MatchesCanonicalTextureBytes(
    const TextureState& texture,
    const std::vector<u8>& canonicalBytes) {
    const size_t byteSize = canonicalBytes.size();
    if (texture.data == nullptr || byteSize == 0u) {
        return false;
    }
    if (texture.sourceEncoding ==
        TextureState::SourceEncoding::CanonicalBigEndian) {
        return std::memcmp(
            canonicalBytes.data(),
            texture.data,
            byteSize) == 0;
    }

    // NativeU16 is an SDK-facing host representation. Compare each word to
    // the canonical GameCube byte snapshot explicitly, without allocating a
    // second full-size vector or interpreting console bytes in host order.
    return MatchesCanonicalNativeU16Bytes(
        texture.data,
        canonicalBytes);
}

bool MatchesCanonicalTlutBytes(
    const TlutState& tlut,
    const std::vector<u8>& canonicalBytes) {
    const bool hasSource =
        !tlut.canonicalBytes.empty() || tlut.data != nullptr;
    if (!hasSource) {
        return canonicalBytes.empty();
    }

    const size_t byteSize = static_cast<size_t>(tlut.entries) * 2u;
    if (byteSize == 0u || canonicalBytes.size() != byteSize) {
        return false;
    }
    if (!tlut.canonicalBytes.empty()) {
        return
            tlut.canonicalBytes.size() == byteSize &&
            std::memcmp(
                canonicalBytes.data(),
                tlut.canonicalBytes.data(),
                byteSize) == 0;
    }
    if (tlut.sourceEncoding == TlutState::SourceEncoding::NativeU16) {
        return MatchesCanonicalNativeU16Bytes(
            tlut.data,
            canonicalBytes);
    }
    return std::memcmp(
        canonicalBytes.data(),
        tlut.data,
        byteSize) == 0;
}

bool CopyCanonicalTlutBytes(
    const TlutState& tlut,
    std::vector<u8>& canonicalBytes) {
    const bool hasSource =
        !tlut.canonicalBytes.empty() || tlut.data != nullptr;
    if (!hasSource) {
        canonicalBytes.clear();
        return true;
    }

    const size_t byteSize = static_cast<size_t>(tlut.entries) * 2u;
    if (byteSize == 0u) {
        canonicalBytes.clear();
        return false;
    }
    if (!tlut.canonicalBytes.empty()) {
        if (tlut.canonicalBytes.size() != byteSize) {
            canonicalBytes.clear();
            return false;
        }
        canonicalBytes = tlut.canonicalBytes;
        return true;
    }
    if (tlut.sourceEncoding == TlutState::SourceEncoding::NativeU16) {
        return CopyCanonicalNativeU16Bytes(
            tlut.data,
            byteSize,
            canonicalBytes);
    }
    canonicalBytes.resize(byteSize);
    std::memcpy(canonicalBytes.data(), tlut.data, byteSize);
    return true;
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
    // GX selects the last resident mip with the integer part of MAX_LOD.
    // Rounding a fractional value up reads a level that GXGetTexBufferSize
    // did not include and can walk past the application's texture payload.
    size_t requestedMaximumLevel = static_cast<size_t>(maximumLod);
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

    if (texture.sourceEncoding ==
        TextureState::SourceEncoding::NativeU16) {
        return CopyCanonicalNativeU16Bytes(
            texture.data,
            byteSize,
            canonicalBytes);
    } else {
        canonicalBytes.resize(byteSize);
        std::memcpy(canonicalBytes.data(), texture.data, byteSize);
    }
    return true;
}

namespace Detail {

bool TextureSourceKey::operator==(
    const TextureSourceKey& other) const noexcept {
    return data == other.data &&
        width == other.width &&
        height == other.height &&
        format == other.format &&
        mipmap == other.mipmap &&
        mipLevelCount == other.mipLevelCount &&
        sourceEncoding == other.sourceEncoding &&
        usesTlut == other.usesTlut &&
        tlutName == other.tlutName &&
        tlutFormat == other.tlutFormat &&
        tlutEntries == other.tlutEntries &&
        tlutRevision == other.tlutRevision &&
        tlutSourceEncoding == other.tlutSourceEncoding;
}

size_t TextureSourceKeyHash::operator()(
    const TextureSourceKey& key) const noexcept {
    size_t hash = std::hash<const void*>{}(key.data);
    const auto combine = [&hash](size_t value) {
        hash ^= value + static_cast<size_t>(0x9e3779b9u) +
            (hash << 6u) + (hash >> 2u);
    };
    combine(key.width);
    combine(key.height);
    combine(static_cast<size_t>(key.format));
    combine(key.mipmap ? 1u : 0u);
    combine(key.mipLevelCount);
    combine(static_cast<size_t>(key.sourceEncoding));
    combine(key.usesTlut ? 1u : 0u);
    combine(key.tlutName);
    combine(static_cast<size_t>(key.tlutFormat));
    combine(key.tlutEntries);
    combine(static_cast<size_t>(key.tlutRevision));
    if constexpr (sizeof(size_t) < sizeof(u64)) {
        combine(static_cast<size_t>(key.tlutRevision >> 32u));
    }
    combine(static_cast<size_t>(key.tlutSourceEncoding));
    return hash;
}

TextureSourceKey MakeTextureSourceKey(
    const TextureState& texture,
    const TlutState* tlut) {
    TextureSourceKey key;
    key.data = texture.data;
    key.width = texture.width;
    key.height = texture.height;
    key.format = texture.format;
    key.mipmap = texture.mipmap;
    key.mipLevelCount = GetTextureMipLevelCount(texture);
    key.sourceEncoding = texture.sourceEncoding;
    key.usesTlut =
        texture.format == GX_TF_C4 ||
        texture.format == GX_TF_C8 ||
        texture.format == GX_TF_C14X2;
    if (key.usesTlut) {
        key.tlutName = texture.tlutName;
        if (tlut != nullptr) {
            key.tlutFormat = tlut->format;
            key.tlutEntries = tlut->entries;
            key.tlutRevision = tlut->revision;
            key.tlutSourceEncoding = tlut->sourceEncoding;
        }
    }
    return key;
}

size_t GetDecodedTextureByteSize(const TextureState& texture) {
    const size_t levelCount = GetTextureMipLevelCount(texture);
    size_t byteSize = 0u;
    for (size_t level = 0u; level < levelCount; ++level) {
        TextureMipLevelLayout layout;
        if (!GetTextureMipLevelLayout(texture, level, layout)) {
            return 0u;
        }
        byteSize +=
            static_cast<size_t>(layout.width) *
            static_cast<size_t>(layout.height) * 4u;
    }
    return byteSize;
}

}

void NotifyTextureCopyDestinationWrite(GlobalState& state) {
    // GXCopyTex writes through the texture cache rather than ordinary CPU
    // stores. The host renderer writes the emulated backing memory directly,
    // so issue the equivalent TMEM invalidation after a successful copy. The
    // content snapshot still prevents redundant decode/upload work when an
    // unrelated texture is subsequently used.
    state.SetBpRegister(0x66000000u);
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

    if (!MatchesCanonicalTextureBytes(texture, mTextureBytes)) {
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
    return
        mTlutFormat == tlutFormat &&
        mTlutEntries == tlutEntries &&
        (tlut == nullptr
             ? mTlutBytes.empty()
             : MatchesCanonicalTlutBytes(*tlut, mTlutBytes));
}

void TextureContentSnapshot::Capture(
    const TextureState& texture,
    const TlutState* tlut) {
    std::vector<u8> canonicalTextureBytes;
    (void)CopyCanonicalTextureBytes(texture, canonicalTextureBytes);
    (void)CaptureCanonical(
        texture, std::move(canonicalTextureBytes), tlut);
}

bool TextureContentSnapshot::CaptureCanonical(
    const TextureState& texture,
    std::vector<u8>&& canonicalTextureBytes,
    const TlutState* tlut) {
    mWidth = texture.width;
    mHeight = texture.height;
    mFormat = texture.format;
    const size_t expectedTextureBytes =
        GetTextureSourceByteSize(texture);
    mValid =
        texture.data != nullptr &&
        expectedTextureBytes != 0u &&
        canonicalTextureBytes.size() == expectedTextureBytes;
    mTextureBytes = std::move(canonicalTextureBytes);

    const bool usesTlut =
        texture.format == GX_TF_C4 ||
        texture.format == GX_TF_C8 ||
        texture.format == GX_TF_C14X2;
    mTlutFormat =
        usesTlut && tlut != nullptr ? tlut->format : GX_TL_IA8;
    mTlutEntries =
        usesTlut && tlut != nullptr ? tlut->entries : 0u;
    if (usesTlut && tlut != nullptr) {
        mValid =
            CopyCanonicalTlutBytes(*tlut, mTlutBytes) &&
            mValid;
    } else {
        mTlutBytes.clear();
    }
    return mValid;
}

}
