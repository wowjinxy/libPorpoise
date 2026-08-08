#ifndef LIBPORPOISE_SIM_GX_TEXTURE_MANAGER_HPP
#define LIBPORPOISE_SIM_GX_TEXTURE_MANAGER_HPP

#include <unordered_map>

#include <dolphin/types.h>

#include "simulator/sim_gx_TextureManager.h"

namespace SIM::GX {

class Texture {
 public:
  Texture();
  Texture(GXTexObjPriv& obj);
  ~Texture();

  const GXTexObjPriv& GetGxTexObj();
  inline u32 GetGlTextureId() {return mGlTextureId;}
  inline bool GetIsConvertedToGl() {return mTextureBuf != nullptr;}

  inline void SetMipmap(u8 mipmap) {mMipmap = mipmap;}
  inline void SetWidth(u16 width) {mWidth = width;}
  inline void SetHeight(u16 height) {mHeight = height;}
  inline void SetWrapS(GXTexWrapMode wrapS) {mWrapS = wrapS;}
  inline void SetWrapT(GXTexWrapMode wrapT) {mWrapT = wrapT;}

  void GenGlTexture();
  void DeleteGlTexture();
  
  void Activate(GXTexMapID mapId);
  void ConvertToGl();
 
 private:
  GXTexObjPriv mGxTexObj;
  u8 mMipmap;
  u16 mWidth;
  u16 mHeight;
  GXTexWrapMode mWrapS;
  GXTexWrapMode mWrapT;
  u32 mGlTextureId; 
  u8 * mTextureBuf; /* Buffer to store the converted texture.*/
};

class TextureManager {
 public:
  TextureManager();
  static TextureManager& GetInstance();

  void InitTexObj(GXTexObj* obj, void* image_ptr, u16 width, u16 height, GXTexFmt format, GXTexWrapMode wrap_s, GXTexWrapMode wrap_t, u8 mipmap);
  void LoadTexObj(GXTexObj* obj, GXTexMapID map);

 private:
  std::unordered_map<void *, Texture> mTextureCache;
  
};

}


#endif
