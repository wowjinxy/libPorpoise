#include "simulator/sim_gx_TextureManager.hpp"
#include "dolphin/gx/GXEnum.h"

#include <dolphin.h>

#include <simulator/glad/glad.h>

static SIM::GX::TextureManager sGXTextureManager = {};

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
  glGenTextures(1, &mGlTextureId);
  glBindTexture(GL_TEXTURE_2D, mGlTextureId);
}

void Texture::DeleteGlTexture() {
  glDeleteTextures(1, &mGlTextureId);
}

void Texture::Activate(GXTexMapID mapId) {
    glActiveTexture(GL_TEXTURE0 + static_cast<int>(mapId));
    glBindTexture(GL_TEXTURE_2D, mGlTextureId);
}

void Texture::ConvertToGl() {
    // Determine output format
    size_t inputBytesPerPixel = 1;
    size_t outputBytesPerPixel = 4;
    GLenum outputGlInternalFormat = GL_RGBA8;
    GLenum outputFormat = GL_RGBA;
    GLenum outputType = GL_UNSIGNED_BYTE;
    bool nativeFormat = false;

    switch(mGxTexObj.format) {
        case GX_TF_I4:
            // This is a special case since there are two pixels per byte
            break;
        case GX_TF_I8:
            inputBytesPerPixel = 1;
            break;
        case GX_TF_IA4:
            // Intensity + alpha 8 bits(4+4).
            inputBytesPerPixel = 1;

            break;
        case GX_TF_IA8:
            // Intensity + alpha 16 bits(8+8)
            inputBytesPerPixel = 2;

            break;
        case GX_TF_RGB565:
            // GL supports this format natively
            inputBytesPerPixel = 2;
            outputBytesPerPixel = 2;
            // NOTES: maybe unsigned_short_5_6_5_rev for bigendian?
            outputFormat = GL_RGB;
            outputType = GL_UNSIGNED_SHORT_5_6_5_REV;
            outputGlInternalFormat = GL_RGB16;
            nativeFormat = true;
            break;
        case GX_TF_RGB5A3:
            // When MSB=1, RGB555 format (opaque), when MSB=0, RGBA4443 format (transparent).

            break;
        default:
        case GX_TF_RGBA8:
            // GL supports this format natively
            inputBytesPerPixel = 4;
            outputBytesPerPixel = 4;
            outputFormat = GL_RGBA;
            outputType = GL_UNSIGNED_BYTE;
            nativeFormat = true;
            break;
        case GX_TF_CMPR:
            // Another special case with 4-bit texels
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

    void * texSourceAddr = mGxTexObj.fullAddress;


    // If the texture is a natively supported format, just copy it
    if(nativeFormat) {
        memcpy(mTextureBuf, texSourceAddr, textureSize);
    } else {
        // Convert to RGBA

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
    GXTexObjPriv* privObj = (GXTexObjPriv*)obj;

    privObj->fullAddress = image_ptr;

    Texture tex = Texture(*privObj);
    tex.SetWidth(width);
    tex.SetHeight(height);
    tex.SetMipmap(mipmap);
    tex.SetWrapS(wrap_s);
    tex.SetWrapT(wrap_t);
    tex.GenGlTexture();

    void * addr = privObj->fullAddress;

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
        texture.ConvertToGl();
    }

    texture.Activate(map);
}


}

// C APIs

void SIM_GX_TextureManager_InitTexObj(GXTexObj* obj, void* image_ptr, u16 width, u16 height, GXTexFmt format, GXTexWrapMode wrap_s, GXTexWrapMode wrap_t, u8 mipmap) {
    auto& manager = SIM::GX::TextureManager::GetInstance();

    manager.InitTexObj(obj, image_ptr, width, height, format, wrap_s, wrap_t, mipmap);
}

void SIM_GX_TextureManager_LoadTexObj(GXTexObj* obj, GXTexMapID map) {
    auto& manager = SIM::GX::TextureManager::GetInstance();

    manager.LoadTexObj(obj, map);
}