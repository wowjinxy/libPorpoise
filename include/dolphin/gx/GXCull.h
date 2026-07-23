#ifndef DOLPHIN_GXCULL_H
#define DOLPHIN_GXCULL_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht);
void GXGetScissor(u32* xOrig, u32* yOrig, u32* wd, u32* ht);
void GXSetCullMode(GXCullMode mode);
void GXGetCullMode(GXCullMode* mode);
void GXSetCoPlanar(GXBool enable);

#ifdef __cplusplus
}
#endif

#endif
