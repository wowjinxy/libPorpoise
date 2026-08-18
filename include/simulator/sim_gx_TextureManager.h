#ifndef LIBPORPOISE_SIM_GX_TEXTURE_MANAGER_H
#define LIBPORPOISE_SIM_GX_TEXTURE_MANAGER_H

#include <dolphin/types.h>


#include <dolphin/gx/GXAttr.h>

#ifdef __cplusplus
extern "C" {
#endif

void SIM_GX_TextureManager_InitTexObj(GXTexObj* obj, void* image_ptr, u16 width, u16 height, GXTexFmt format, GXTexWrapMode wrap_s, GXTexWrapMode wrap_t, u8 mipmap);
void SIM_GX_TextureManager_LoadTexObj(GXTexObj* obj, GXTexMapID map);

#ifdef __cplusplus
}
#endif

#endif