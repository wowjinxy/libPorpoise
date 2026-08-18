#include "simulator/sim_gx_TextureManager.hpp"
#include "dolphin/gx/GXEnum.h"

#include <dolphin.h>

#include <simulator/sim_gx_Thread.hpp>
#include <simulator/glad/glad.h>

static SIM::GX::TextureManager sGXTextureManager = {};

// Texture conversion functions
static void ByteSwapRGB565(u8 * in, u8* out, u16 width, u16 height) {
    // 4x4 tiled blocks, 16bpp
    const int blockW = 4, blockH = 4;
    int bw = (width + blockW - 1) / blockW;
    int bh = (height + blockH - 1) / blockH;

    u16 * inPtr = (u16*)(in);
    u16 * outPtr = (u16*)(out);

    for (int by = 0; by < bh; by++) {
        for (int bx = 0; bx < bw; bx++) {
            for (int y = 0; y < blockH; y++) {
                for (int x = 0; x < blockW; x++) {
                    u16 inputVal = *inPtr++;
                    int px = bx * blockW + x;
                    int py = by * blockH + y;

                    u8 r = (inputVal & 0xF8) >> 11;
                    u8 g = (inputVal & 0x07E0) >> 5;
                    u8 b = (inputVal & 0x1F);

                    u16 outVal = (r) | (g << 5) | (b << 11);

                    if (px < width && py < height) {
                        outPtr[py * width + px] = outVal;
                    }
                }
            }
        }
    }
}

static void ByteSwapRGBA(u8 * in, u8 * out, u16 width, u16 height) {
    // 4x4 tiled blocks, 32bpp (two passes per block: AR then GB)
    const int blockW = 4, blockH = 4;
    int bw = (width + blockW - 1) / blockW;
    int bh = (height + blockH - 1) / blockH;

    u8 * inPtr = in;
    u32 * outPtr = (u32*)out;

    for (int by = 0; by < bh; by++) {
        for (int bx = 0; bx < bw; bx++) {
            u8 ar[16][2];
            for (int i = 0; i < 16; i++) {
                ar[i][0] = *inPtr++; // A
                ar[i][1] = *inPtr++; // R
            }
            for (int i = 0; i < 16; i++) {
                u8 g = *inPtr++;
                u8 b = *inPtr++;
                int x = i % blockW, y = i / blockW;
                int px = bx * blockW + x;
                int py = by * blockH + y;
                if (px < width && py < height) {
                    u8 r = ar[i][1], a = ar[i][0];
                    outPtr[py * width + px] = (r) | (g << 8) | (b << 16) | (a << 24);
                }
            }
        }
    }
}

static void ConvertI4(u8* in, u8 * out, u16 width, u16 height) {
    // 8x8 tiled blocks, 4bpp (2 pixels per byte)
    const int blockW = 8, blockH = 8;
    int bw = (width + blockW - 1) / blockW;
    int bh = (height + blockH - 1) / blockH;

    u8 * inPtr = in;
    u32 * outPtr = (u32*)out;

    for (int by = 0; by < bh; by++) {
        for (int bx = 0; bx < bw; bx++) {
            for (int y = 0; y < blockH; y++) {
                for (int x = 0; x < blockW; x += 2) {
                    u8 inputVal = *inPtr++;
                    u8 upper = (inputVal & 0xF0) | (inputVal >> 4);
                    u8 lower = (inputVal & 0x0F) | ((inputVal & 0x0F) << 4);

                    int px0 = bx * blockW + x;
                    int px1 = px0 + 1;
                    int py = by * blockH + y;

                    if (px0 < width && py < height) {
                        outPtr[py * width + px0] = (upper) | (upper << 8) | (upper << 16) | (upper << 24);
                    }
                    if (px1 < width && py < height) {
                        outPtr[py * width + px1] = (lower) | (lower << 8) | (lower << 16) | (lower << 24);
                    }
                }
            }
        }
    }
}

static void ConvertI8(u8* in, u8 * out, u16 width, u16 height) {
    // 8x4 tiled blocks, 8bpp
    const int blockW = 8, blockH = 4;
    int bw = (width + blockW - 1) / blockW;
    int bh = (height + blockH - 1) / blockH;

    u8 * inPtr = in;
    u32 * outPtr = (u32*)out;

    for (int by = 0; by < bh; by++) {
        for (int bx = 0; bx < bw; bx++) {
            for (int y = 0; y < blockH; y++) {
                for (int x = 0; x < blockW; x++) {
                    u8 inputVal = *inPtr++;
                    int px = bx * blockW + x;
                    int py = by * blockH + y;
                    if (px < width && py < height) {
                        outPtr[py * width + px] = (inputVal) | (inputVal << 8) | (inputVal << 16) | (inputVal << 24);
                    }
                }
            }
        }
    }
}

static void ConvertIA4(u8* in, u8* out, u16 width, u16 height)
{
    const int blockW = 8, blockH = 4;

    int bw = (width + blockW - 1) / blockW;
    int bh = (height + blockH - 1) / blockH;

    u8* inPtr = in;
    u32* outPtr = (u32*)out;

    for (int by = 0; by < bh; by++)
    {
        for (int bx = 0; bx < bw; bx++)
        {
            for (int y = 0; y < blockH; y++)
            {
                for (int x = 0; x < blockW; x++)
                {
                    u8 v = *inPtr++;

                    u8 intensity = (v >> 4);
                    intensity |= intensity << 4;

                    u8 alpha = v & 0xF;
                    alpha |= alpha << 4;

                    int px = bx * blockW + x;
                    int py = by * blockH + y;

                    if (px < width && py < height)
                    {
                        outPtr[py * width + px] =
                            intensity |
                            (intensity << 8) |
                            (intensity << 16) |
                            (alpha << 24);
                    }
                }
            }
        }
    }
}

static void ConvertIA8(u8* in, u8* out, u16 width, u16 height)
{
    const int blockW = 4;
    const int blockH = 4;

    int bw = (width + blockW - 1) / blockW;
    int bh = (height + blockH - 1) / blockH;

    u8* inPtr = in;
    u32* outPtr = (u32*)out;

    for (int by = 0; by < bh; by++)
    {
        for (int bx = 0; bx < bw; bx++)
        {
            for (int y = 0; y < blockH; y++)
            {
                for (int x = 0; x < blockW; x++)
                {
                    u8 intensity = *inPtr++;
                    u8 alpha = *inPtr++;

                    int px = bx * blockW + x;
                    int py = by * blockH + y;

                    if (px < width && py < height)
                    {
                        outPtr[py * width + px] =
                            intensity |
                            (intensity << 8) |
                            (intensity << 16) |
                            (alpha << 24);
                    }
                }
            }
        }
    }
}

static inline u32 DecodeRGB5A3(u16 v)
{
    if (v & 0x8000)
    {
        // RGB555
        u8 r = ((v >> 10) & 0x1F) * 255 / 31;
        u8 g = ((v >> 5) & 0x1F) * 255 / 31;
        u8 b = (v & 0x1F) * 255 / 31;

        return r | (g << 8) | (b << 16) | 0xFF000000;
    }

    u8 a = ((v >> 12) & 0x7) * 255 / 7;
    u8 r = ((v >> 8) & 0xF) * 255 / 15;
    u8 g = ((v >> 4) & 0xF) * 255 / 15;
    u8 b = (v & 0xF) * 255 / 15;

    return r | (g << 8) | (b << 16) | (a << 24);
}

static void ConvertRGB5A3(u8* in, u8* out, u16 width, u16 height)
{
    const int blockW = 4;
    const int blockH = 4;

    int bw = (width + blockW - 1) / blockW;
    int bh = (height + blockH - 1) / blockH;

    u16* inPtr = (u16*)in;
    u32* outPtr = (u32*)out;

    for (int by = 0; by < bh; by++)
    {
        for (int bx = 0; bx < bw; bx++)
        {
            for (int y = 0; y < blockH; y++)
            {
                for (int x = 0; x < blockW; x++)
                {
                    u16 v = *inPtr++;

                    v = (v << 8) | (v >> 8);

                    int px = bx * blockW + x;
                    int py = by * blockH + y;

                    if (px < width && py < height)
                        outPtr[py * width + px] = DecodeRGB5A3(v);
                }
            }
        }
    }
}

static inline u32 RGB565ToRGBA8888(u16 c)
{
    // GX texture data is big-endian
    c = (c << 8) | (c >> 8);

    u8 r = ((c >> 11) & 0x1F) * 255 / 31;
    u8 g = ((c >> 5)  & 0x3F) * 255 / 63;
    u8 b = ( c        & 0x1F) * 255 / 31;

    return r | (g << 8) | (b << 16) | 0xFF000000;
}

static void DecodeDXT1Block(
    const u8* block,
    u32* out,
    u16 width,
    int dstX,
    int dstY)
{
    u16 c0 = *(const u16*)(block + 0);
    u16 c1 = *(const u16*)(block + 2);

    u32 palette[4];

    palette[0] = RGB565ToRGBA8888(c0);
    palette[1] = RGB565ToRGBA8888(c1);

    auto expand = [](u32 c, int channel)
    {
        return (c >> (channel * 8)) & 0xFF;
    };

    if (c0 > c1)
    {
        u8 r0 = expand(palette[0],0), g0 = expand(palette[0],1), b0 = expand(palette[0],2);
        u8 r1 = expand(palette[1],0), g1 = expand(palette[1],1), b1 = expand(palette[1],2);

        palette[2] =
            ((2*r0+r1)/3) |
            (((2*g0+g1)/3) << 8) |
            (((2*b0+b1)/3) << 16) |
            0xFF000000;

        palette[3] =
            ((r0+2*r1)/3) |
            (((g0+2*g1)/3) << 8) |
            (((b0+2*b1)/3) << 16) |
            0xFF000000;
    }
    else
    {
        u8 r0 = expand(palette[0],0), g0 = expand(palette[0],1), b0 = expand(palette[0],2);
        u8 r1 = expand(palette[1],0), g1 = expand(palette[1],1), b1 = expand(palette[1],2);

        palette[2] =
            (((r0+r1)/2)) |
            ((((g0+g1)/2)) << 8) |
            ((((b0+b1)/2)) << 16) |
            0xFF000000;

        palette[3] = 0;
    }

    u32 indices = *(const u32*)(block + 4);

    // indices are also big-endian
    indices =
        ((indices & 0x000000FF) << 24) |
        ((indices & 0x0000FF00) << 8)  |
        ((indices & 0x00FF0000) >> 8)  |
        ((indices & 0xFF000000) >> 24);

    for (int y = 0; y < 4; y++)
    {
        for (int x = 0; x < 4; x++)
        {
            int idx = (indices >> (30 - 2*(y*4+x))) & 3;

            int px = dstX + x;
            int py = dstY + y;

            out[py * width + px] = palette[idx];
        }
    }
}

static void ConvertCMPR(u8* in, u8* out, u16 width, u16 height)
{
    const int tileW = 8;
    const int tileH = 8;

    int tilesX = (width  + tileW - 1) / tileW;
    int tilesY = (height + tileH - 1) / tileH;

    const u8* inPtr = in;
    u32* outPtr = (u32*)out;

    for (int ty = 0; ty < tilesY; ty++)
    {
        for (int tx = 0; tx < tilesX; tx++)
        {
            // top-left
            DecodeDXT1Block(
                inPtr,
                outPtr,
                width,
                tx * 8,
                ty * 8);
            inPtr += 8;

            // top-right
            DecodeDXT1Block(
                inPtr,
                outPtr,
                width,
                tx * 8 + 4,
                ty * 8);
            inPtr += 8;

            // bottom-left
            DecodeDXT1Block(
                inPtr,
                outPtr,
                width,
                tx * 8,
                ty * 8 + 4);
            inPtr += 8;

            // bottom-right
            DecodeDXT1Block(
                inPtr,
                outPtr,
                width,
                tx * 8 + 4,
                ty * 8 + 4);
            inPtr += 8;
        }
    }
}

namespace SIM::GX {

// Texture
Texture::Texture() : Texture({}) {};

Texture::Texture(GXTexObjPriv& obj) {
    mGxTexObj = obj;
    mTextureBuf = nullptr;
}

Texture::~Texture() {
    if(mTextureBuf != nullptr) {
        delete mTextureBuf;
    }
}

void Texture::GenGlTexture() {
  glGenTextures(1, (GLuint*)&mGlTextureId);
  glBindTexture(GL_TEXTURE_2D, mGlTextureId);
}

void Texture::DeleteGlTexture() {
  glDeleteTextures(1, (GLuint*)&mGlTextureId);
}

void Texture::Activate(GXTexMapID mapId) {
    glActiveTexture(GL_TEXTURE0 + static_cast<int>(mapId));
    glBindTexture(GL_TEXTURE_2D, mGlTextureId);
}

void Texture::ConvertToGl() {
    // Determine output format
    size_t outputBytesPerPixel = 4;
    GLenum outputGlInternalFormat = GL_RGBA8;
    GLenum outputFormat = GL_RGBA;
    GLenum outputType = GL_UNSIGNED_BYTE;

    void (*conversionFunc)(u8*, u8*, u16, u16) = nullptr;

    switch(mGxTexObj.format) {
        case GX_TF_I4:
            // This is a special case since there are two pixels per byte
            conversionFunc = ConvertI4;
            break;
        case GX_TF_I8:
            conversionFunc = ConvertI8;
            break;
        case GX_TF_IA4:
            // Intensity + alpha 8 bits(4+4).
            conversionFunc = ConvertIA4;

            break;
        case GX_TF_IA8:
            // Intensity + alpha 16 bits(8+8)
            conversionFunc = ConvertIA8;

            break;
        case GX_TF_RGB565:
            outputBytesPerPixel = 2;
            // NOTES: maybe unsigned_short_5_6_5_rev for bigendian?
            outputFormat = GL_RGB;
            outputType = GL_UNSIGNED_SHORT_5_6_5_REV;
            //outputGlInternalFormat = GL_RGB16;
            conversionFunc = ByteSwapRGB565;
            break;
        case GX_TF_RGB5A3:
            // When MSB=1, RGB555 format (opaque), when MSB=0, RGBA4443 format (transparent).
            conversionFunc = ConvertRGB5A3;

            break;
        default:
        case GX_TF_RGBA8:
            outputBytesPerPixel = 4;
            outputFormat = GL_RGBA;
            outputType = GL_UNSIGNED_BYTE;
            conversionFunc = ByteSwapRGBA;
            break;
        case GX_TF_CMPR:
            // Another special case with 4-bit texels
            conversionFunc = ConvertCMPR;
            break;
        
        case GX_TF_Z8:
            break;
        case GX_TF_Z16:
            break;
        case GX_TF_Z24X8:
            break;
    }


    size_t textureSize = mWidth * mHeight * outputBytesPerPixel;

    if(mTextureBuf) {
        OSReport("TextureManager Warning: converting a texture to GL that has already been converted.\n");
        delete mTextureBuf;
        mTextureBuf = nullptr;
    }

    mTextureBuf = new u8[textureSize];

    u8 * texSourceAddr = (u8*)mGxTexObj.fullAddress;


    // Perform texture conversion
    if(conversionFunc) {
        conversionFunc(texSourceAddr, mTextureBuf, mWidth, mHeight);
    } else {
        // No conversion function defined, just perform a copy
        // Note that this will likely result in an incorrect texture
        memcpy(mTextureBuf, texSourceAddr, textureSize);
    }

    glBindTexture(GL_TEXTURE_2D, mGlTextureId);
    int error = glGetError();

    //if(error != GL_NO_ERROR) {
    //    OSReport("TextureManager: glBindTexture error %d\n", error);
    //}

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, outputGlInternalFormat, mWidth, mHeight, 0, outputFormat, outputType, mTextureBuf);
    
    error = glGetError();

    if(error != GL_NO_ERROR) {
        OSReport("TextureManager: glTexImage2D error %d\n", error);
    }
}


// Texture Manager
TextureManager::TextureManager() {
    mTextureCache = {};
}

TextureManager& TextureManager::GetInstance() {
    return sGXTextureManager;
}

void TextureManager::InitTexObj(GXTexObj* obj, void* image_ptr, u16 width, u16 height, GXTexFmt format, GXTexWrapMode wrap_s, GXTexWrapMode wrap_t, u8 mipmap) {
    // This needs to happen on the GX thread
    GXTexObjPriv* privObj = (GXTexObjPriv*)obj;

    privObj->fullAddress = image_ptr;

    Texture tex = Texture(*privObj);
    tex.SetWidth(width);
    tex.SetHeight(height);
    tex.SetMipmap(mipmap);
    tex.SetWrapS(wrap_s);
    tex.SetWrapT(wrap_t);

    void * addr = privObj->fullAddress;

    // Remove the existing texture
    if(mTextureCache.count(addr) > 0) {
        mTextureCache[addr].DeleteGlTexture();
        mTextureCache.erase(addr);
    }

    mTextureCache.emplace(addr, std::move(tex));
}

void TextureManager::LoadTexObj(GXTexObj* obj, GXTexMapID map) {
    GXTexObjPriv* privObj = (GXTexObjPriv*)obj;
    void * addr = privObj->fullAddress;
    
    if(mTextureCache.count(addr) == 0) {
        // TexObj is not in our cache, it was not initialized
        OSReport("TextureManager: Tried to load a TexObj that is not in the cache\n");
        return;
    }

    auto& texture = mTextureCache[addr];

    if(!texture.GetIsConvertedToGl()) {
        texture.GenGlTexture();
        texture.ConvertToGl();
    }

    texture.Activate(map);
}


}

// C APIs
void SIM_GX_TextureManager_InitTexObj(GXTexObj* obj, void* image_ptr, u16 width, u16 height, GXTexFmt format, GXTexWrapMode wrap_s, GXTexWrapMode wrap_t, u8 mipmap) {
    // Send a message to the GX thread
    SIM::GX::ThreadMessage msg;
    msg.mType = SIM::GX::ThreadMessageType::InitTexObj;

    msg.mInitTexObj.obj = obj;
    msg.mInitTexObj.imagePtr = image_ptr;
    msg.mInitTexObj.width = width;
    msg.mInitTexObj.height = height;
    msg.mInitTexObj.format = format;
    msg.mInitTexObj.wrapS = wrap_s;
    msg.mInitTexObj.wrapT = wrap_t;
    msg.mInitTexObj.mipmap = mipmap;
    SIM::GX::SendThreadMessage(msg);
}

void SIM_GX_TextureManager_LoadTexObj(GXTexObj* obj, GXTexMapID map) {
    //auto& manager = SIM::GX::TextureManager::GetInstance();
//
    //manager.LoadTexObj(obj, map);

    SIM::GX::ThreadMessage msg;
    msg.mType = SIM::GX::ThreadMessageType::LoadTexObj;

    msg.mLoadTexObj.obj = obj;
    msg.mLoadTexObj.map = map;
    SIM::GX::SendThreadMessage(msg);
}