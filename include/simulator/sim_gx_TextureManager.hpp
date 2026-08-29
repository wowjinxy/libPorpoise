#ifndef LIBPORPOISE_SIM_GX_TEXTURE_MANAGER_HPP
#define LIBPORPOISE_SIM_GX_TEXTURE_MANAGER_HPP

#include <array>
#include <unordered_map>

#include <dolphin/types.h>

#include "simulator/sim_gx_TextureManager.h"

namespace SIM::GX {

class Texture {
 public:
  Texture();
  ~Texture();

  inline u32 GetGlTextureId() {return mGlTextureId;}
  inline bool GetIsConvertedToGl() {return mTextureBuf != nullptr;}
  size_t GetSourceBufSize();

  inline void SetMipmap(u8 mipmap) {mMipmap = mipmap;}
  inline void SetWidth(u16 width) {mWidth = width;}
  inline void SetHeight(u16 height) {mHeight = height;}
  inline void SetWrapS(GXTexWrapMode wrapS) {mWrapS = wrapS;}
  inline void SetWrapT(GXTexWrapMode wrapT) {mWrapT = wrapT;}

  void GenGlTexture();
  void DeleteGlTexture();
  
  void Activate(GXTexMapID mapId);
  void ConvertToGl(GXTexMapID mapId);
 

  u8 mMipmap;
  u16 mWidth;
  u16 mHeight;
  GXTexWrapMode mWrapS;
  GXTexWrapMode mWrapT;
  u32 mGlTextureId; 
  GXTexFmt mSourceFormat;
  u8 * mSourceData;
  u8 * mTextureBuf; /* Buffer to store the converted texture.*/
};

class TextureManager {
 public:
  TextureManager();
  static TextureManager& GetInstance();

  void ProcessTextures();

 private:
  std::array<Texture, GX_MAX_TEXMAP> mLoadedTextures = {};
  
  // Converted texture data is hashed by the CRC of the
  // source texture data
  std::unordered_map<u32, Texture> mTextureCache;
  
};

}


#endif
