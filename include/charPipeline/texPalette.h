#ifndef TEXPALETTE_H
#define TEXPALETTE_H

#include <dolphin/tpl.h>

/*
 * Dolphin's demo library used TEX names for the palette structures later
 * exposed by Revolution as TPL. Both APIs share one canonical layout and
 * implementation in libPorpoise.
 */
typedef TPLClutHeader CLUTHeader;
typedef TPLClutHeaderPtr CLUTHeaderPtr;
typedef TPLHeader TEXHeader;
typedef TPLHeaderPtr TEXHeaderPtr;
typedef TPLDescriptor TEXDescriptor;
typedef TPLDescriptorPtr TEXDescriptorPtr;
typedef TPLPalette TEXPalette;
typedef TPLPalettePtr TEXPalettePtr;

#endif
