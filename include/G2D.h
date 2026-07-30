#ifndef G2DAPI_H
#define G2DAPI_H

#include <dolphin/gx.h>

BEGIN_SCOPE_EXTERN_C

typedef enum G2DMatCtg {
	G2D_CTG_TEXTURE     = 0,
	G2D_CTG_RGB_DIRECT  = 1,
	G2D_CTG_RGBA_INDEX8 = 2,
	G2D_CTG_EMPTY       = 3
} G2DMatCtg;

typedef struct G2DMatDesc {
	s32 nReserved;
	G2DMatCtg nCategory;
	GXColor* color;
	GXTexObj* to;
	u8* clut;
} G2DMatDesc;

typedef struct G2DTileDesc {
	u8 nMaterial;
	u8 nS;
	u8 nT;
	u8 nCI;
	u8 aUser[4];
} G2DTileDesc;

typedef struct G2DLayer {
	void* map;
	s8 nHS;
	s8 nVS;
	s8 nBPI;
	s16 nTileWidth;
	s16 nTileHeight;
	s8 bWrap;
	u8 nNumMaterials;
	G2DTileDesc* tileDesc;
	G2DMatDesc* matDesc;
} G2DLayer;

typedef struct G2DSprite {
	u16 nTlcS;
	u16 nTlcT;
	u16 nWidth;
	u16 nHeight;
	GXTexObj* to;
	f32 rS0;
	f32 rT0;
	f32 rS1;
	f32 rT1;
} G2DSprite;

typedef struct G2DPosOri {
	f32 rPosX;
	f32 rPosY;
	f32 rOriX;
	f32 rOriY;
} G2DPosOri;

void G2DInitSprite(G2DSprite* sprite);
void G2DDrawSprite(G2DSprite* sprite, G2DPosOri* posOri);
void G2DDrawLayer(G2DLayer* layer, s8* sortBuffer);
void G2DSetCamera(G2DPosOri* posOri);
void G2DInitWorld(u32 worldWidth, u32 worldHeight);
void G2DSetViewport(u16 left, u16 top, u16 width, u16 height);

END_SCOPE_EXTERN_C

#endif
