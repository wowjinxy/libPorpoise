#ifndef DOLPHIN_GXVERT_H
#define DOLPHIN_GXVERT_H

#include <dolphin/gx/GXFifo.h>

/* Packing macros for color formats (SDK-compatible) */
#define GXPackedRGB565(r, g, b) \
  ((u16)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3)))
#define GXPackedRGBA4(r, g, b, a) \
  ((u16)((((r) & 0xF0) << 8) | (((g) & 0xF0) << 4) | (((b) & 0xF0)) | (((a) & 0xF0) >> 4)))
#define GXPackedRGB5A3(r, g, b, a) \
  ((u16)((a) >= 224 \
             ? ((((r) & 0xF8) << 7) | (((g) & 0xF8) << 2) | (((b) & 0xF8) >> 3) | (1 << 15)) \
             : ((((r) & 0xF0) << 4) | ((g) & 0xF0) | (((b) & 0xF0) >> 4) | (((a) & 0xE0) << 7))))

#endif
