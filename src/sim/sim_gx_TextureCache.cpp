#include <simulator/sim_gx_GlRenderer.hpp>

#include <cstring>

#include <simulator/sim_gx_State.hpp>

namespace SIM::GX {

namespace {

// GX texture image dimensions are encoded as 10-bit (size minus one) fields.
// Reject host descriptors that cannot exist on the original hardware before
// calculating source ranges or allocating an RGBA decode buffer.
constexpr size_t kMaxTextureDimension = 1024u;

}

size_t GetTextureSourceByteSize(const TextureState& texture) {
    if (texture.width == 0u || texture.height == 0u ||
        texture.width > kMaxTextureDimension ||
        texture.height > kMaxTextureDimension) {
        return 0u;
    }

    size_t blockWidth = 0;
    size_t blockHeight = 0;
    size_t bytesPerBlock = 0;
    switch (texture.format) {
        case GX_TF_I4:
        case static_cast<GXTexFmt>(GX_TF_C4):
        case GX_TF_CMPR:
            blockWidth = 8u;
            blockHeight = 8u;
            bytesPerBlock = 32u;
            break;
        case GX_TF_I8:
        case GX_TF_IA4:
        case static_cast<GXTexFmt>(GX_TF_C8):
        case GX_TF_Z8:
            blockWidth = 8u;
            blockHeight = 4u;
            bytesPerBlock = 32u;
            break;
        case GX_TF_IA8:
        case GX_TF_RGB565:
        case GX_TF_RGB5A3:
        case static_cast<GXTexFmt>(GX_TF_C14X2):
        case GX_TF_Z16:
            blockWidth = 4u;
            blockHeight = 4u;
            bytesPerBlock = 32u;
            break;
        case GX_TF_RGBA8:
        case GX_TF_Z24X8:
            blockWidth = 4u;
            blockHeight = 4u;
            bytesPerBlock = 64u;
            break;
        default:
            return 0;
    }

    const size_t blockColumns =
        (static_cast<size_t>(texture.width) + blockWidth - 1u) /
        blockWidth;
    const size_t blockRows =
        (static_cast<size_t>(texture.height) + blockHeight - 1u) /
        blockHeight;
    return blockColumns * blockRows * bytesPerBlock;
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
        const u8 intensity = static_cast<u8>(packed >> 8u);
        color.red = intensity;
        color.green = intensity;
        color.blue = intensity;
        color.alpha = static_cast<u8>(packed & 0xffu);
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
        mTextureBytes.size() != textureByteSize ||
        std::memcmp(
            mTextureBytes.data(),
            texture.data,
            textureByteSize) != 0) {
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
    const size_t textureByteSize = GetTextureSourceByteSize(texture);
    mValid = texture.data != nullptr && textureByteSize != 0u;
    mWidth = texture.width;
    mHeight = texture.height;
    mFormat = texture.format;
    if (mValid) {
        const auto* bytes = static_cast<const u8*>(texture.data);
        mTextureBytes.assign(bytes, bytes + textureByteSize);
    } else {
        mTextureBytes.clear();
    }

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
