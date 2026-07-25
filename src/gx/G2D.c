#include <G2D.h>

#include <dolphin/mtx.h>

#include <stddef.h>

typedef struct G2DState {
	G2DPosOri camera;
	f32 worldWidth;
	f32 worldHeight;
	f32 halfWorldWidth;
	f32 halfWorldHeight;
	u16 viewportLeft;
	u16 viewportTop;
	u16 viewportWidth;
	u16 viewportHeight;
} G2DState;

static G2DState g2dState = {
	{0.0f, 0.0f, 0.0f, 1.0f},
	640.0f,
	448.0f,
	320.0f,
	224.0f,
	0,
	0,
	640,
	448
};

static u32 G2DGetTileIndex(const G2DLayer* layer, u32 index)
{
	if (layer->nBPI == 1) {
		return ((const u8*)layer->map)[index];
	}
	return ((const u16*)layer->map)[index];
}

static void G2DEmitPosition(f32 x, f32 y)
{
	GXPosition2f32(x, y);
}

static void G2DEmitTexturedQuad(const G2DLayer* layer,
	                            const G2DTileDesc* tile,
	                            const G2DMatDesc* material,
	                            f32 x,
	                            f32 y)
{
	const f32 width = (f32)layer->nTileWidth;
	const f32 height = (f32)layer->nTileHeight;
	const f32 inverseTextureWidth =
	    1.0f / (f32)GXGetTexObjWidth(material->to);
	const f32 inverseTextureHeight =
	    1.0f / (f32)GXGetTexObjHeight(material->to);
	const f32 s0 = (f32)tile->nS * width * inverseTextureWidth;
	const f32 t0 = (f32)tile->nT * height * inverseTextureHeight;
	const f32 s1 = s0 + width * inverseTextureWidth;
	const f32 t1 = t0 + height * inverseTextureHeight;

	G2DEmitPosition(x + width, y);
	GXTexCoord2f32(s1, t0);
	G2DEmitPosition(x + width, y + height);
	GXTexCoord2f32(s1, t1);
	G2DEmitPosition(x, y + height);
	GXTexCoord2f32(s0, t1);
	G2DEmitPosition(x, y);
	GXTexCoord2f32(s0, t0);
}

static void G2DEmitColorQuad(const G2DLayer* layer,
	                         const G2DTileDesc* tile,
	                         const G2DMatDesc* material,
	                         f32 x,
	                         f32 y)
{
	GXColor color;
	const f32 width = (f32)layer->nTileWidth;
	const f32 height = (f32)layer->nTileHeight;

	if (material->nCategory == G2D_CTG_RGBA_INDEX8 && material->clut != NULL) {
		color = ((const GXColor*)material->clut)[tile->nCI];
	} else {
		color.r = tile->nS;
		color.g = tile->nT;
		color.b = tile->nCI;
		color.a = 255;
	}

	G2DEmitPosition(x + width, y);
	GXColor4u8(color.r, color.g, color.b, color.a);
	G2DEmitPosition(x + width, y + height);
	GXColor4u8(color.r, color.g, color.b, color.a);
	G2DEmitPosition(x, y + height);
	GXColor4u8(color.r, color.g, color.b, color.a);
	G2DEmitPosition(x, y);
	GXColor4u8(color.r, color.g, color.b, color.a);
}

static GXBool G2DSetMaterialState(const G2DMatDesc* material)
{
	GXClearVtxDesc();
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
	               GX_LO_CLEAR);

	switch (material->nCategory) {
	case G2D_CTG_TEXTURE:
		if (material->to == NULL) {
			return GX_FALSE;
		}
		GXLoadTexObj(material->to, GX_TEXMAP0);
		GXSetNumTexGens(1);
		GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
		GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
		if (material->color != NULL) {
			GXSetNumChans(1);
			GXSetChanMatColor(GX_COLOR0A0, *material->color);
			GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG,
			              GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
			GXSetTevOp(GX_TEVSTAGE0, GX_MODULATE);
			GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0,
			              GX_COLOR0A0);
		} else {
			GXSetNumChans(0);
			GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);
			GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0,
			              GX_COLOR_NULL);
		}
		return GX_TRUE;

	case G2D_CTG_RGB_DIRECT:
	case G2D_CTG_RGBA_INDEX8:
		GXSetNumTexGens(0);
		GXSetNumChans(1);
		GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
		GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
		GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_VTX, GX_SRC_VTX,
		              GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
		GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
		GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL,
		              GX_COLOR0A0);
		return GX_TRUE;

	default:
		return GX_FALSE;
	}
}

static void G2DDrawMaterialRow(const G2DLayer* layer,
	                           u32 materialIndex,
	                           u32 y,
	                           f32 offsetX,
	                           f32 offsetY)
{
	const u32 width = (u32)1 << layer->nHS;
	const u32 rowBase = y * width;
	const G2DMatDesc* material = &layer->matDesc[materialIndex];
	u32 matching = 0;
	u32 x;

	for (x = 0; x < width; ++x) {
		const u32 tileIndex = G2DGetTileIndex(layer, rowBase + x);
		if (layer->tileDesc[tileIndex].nMaterial == materialIndex) {
			++matching;
		}
	}

	x = 0;
	while (matching != 0) {
		const u32 chunk = matching > 16383 ? 16383 : matching;
		u32 emitted = 0;

		GXBegin(GX_QUADS, GX_VTXFMT0, (u16)(chunk * 4));
		while (x < width && emitted < chunk) {
			const u32 tileIndex = G2DGetTileIndex(layer, rowBase + x);
			const G2DTileDesc* tile = &layer->tileDesc[tileIndex];
			if (tile->nMaterial == materialIndex) {
				const f32 tileX =
				    offsetX + (f32)((s32)x * layer->nTileWidth);
				const f32 tileY =
				    offsetY + (f32)((s32)y * layer->nTileHeight);
				if (material->nCategory == G2D_CTG_TEXTURE) {
					G2DEmitTexturedQuad(layer, tile, material, tileX, tileY);
				} else {
					G2DEmitColorQuad(layer, tile, material, tileX, tileY);
				}
				++emitted;
			}
			++x;
		}
		GXEnd();
		matching -= emitted;
		if (emitted == 0) {
			break;
		}
	}
}

void G2DInitSprite(G2DSprite* sprite)
{
	f32 inverseWidth;
	f32 inverseHeight;

	if (sprite == NULL || sprite->to == NULL) {
		return;
	}
	inverseWidth = 1.0f / (f32)GXGetTexObjWidth(sprite->to);
	inverseHeight = 1.0f / (f32)GXGetTexObjHeight(sprite->to);
	sprite->rS0 = ((f32)sprite->nTlcS + 0.5f) * inverseWidth;
	sprite->rT0 = ((f32)sprite->nTlcT + 0.5f) * inverseHeight;
	sprite->rS1 =
	    ((f32)sprite->nTlcS + (f32)sprite->nWidth - 0.5f) * inverseWidth;
	sprite->rT1 =
	    ((f32)sprite->nTlcT + (f32)sprite->nHeight - 0.5f) * inverseHeight;
}

void G2DDrawSprite(G2DSprite* sprite, G2DPosOri* posOri)
{
	f32 halfOriX;
	f32 halfOriY;
	f32 widthX;
	f32 widthY;
	f32 heightX;
	f32 heightY;
	f32 relativeX;
	f32 relativeY;

	if (sprite == NULL || sprite->to == NULL || posOri == NULL) {
		return;
	}

	GXClearVtxDesc();
	GXLoadTexObj(sprite->to, GX_TEXMAP0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetNumTexGens(1);
	GXSetNumChans(0);
	GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
	               GX_LO_CLEAR);

	halfOriX = 0.5f * posOri->rOriX;
	halfOriY = 0.5f * posOri->rOriY;
	widthX = (f32)sprite->nWidth * halfOriX;
	widthY = (f32)sprite->nWidth * halfOriY;
	heightX = (f32)sprite->nHeight * halfOriX;
	heightY = (f32)sprite->nHeight * halfOriY;

	relativeX = posOri->rPosX - g2dState.camera.rPosX;
	relativeY = posOri->rPosY - g2dState.camera.rPosY;
	if (relativeX >= g2dState.halfWorldWidth) {
		relativeX -= g2dState.worldWidth;
	} else if (relativeX < -g2dState.halfWorldWidth) {
		relativeX += g2dState.worldWidth;
	}
	if (relativeY >= g2dState.halfWorldHeight) {
		relativeY -= g2dState.worldHeight;
	} else if (relativeY < -g2dState.halfWorldHeight) {
		relativeY += g2dState.worldHeight;
	}
	relativeX += g2dState.camera.rPosX;
	relativeY += g2dState.camera.rPosY;

	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition2f32(widthY + relativeX - heightX,
	               relativeY - heightY - widthX);
	GXTexCoord2f32(sprite->rS0, sprite->rT1);
	GXPosition2f32(widthY + relativeX + heightX,
	               relativeY + heightY - widthX);
	GXTexCoord2f32(sprite->rS0, sprite->rT0);
	GXPosition2f32(relativeX + heightX - widthY,
	               widthX + relativeY + heightY);
	GXTexCoord2f32(sprite->rS1, sprite->rT0);
	GXPosition2f32(relativeX - heightX - widthY,
	               widthX + relativeY - heightY);
	GXTexCoord2f32(sprite->rS1, sprite->rT1);
	GXEnd();
}

void G2DDrawLayer(G2DLayer* layer, s8* sortBuffer)
{
	u32 material;
	u32 y;
	s32 copyX;
	s32 copyY;
	const u32 height =
	    layer != NULL && layer->nVS >= 0 && layer->nVS < 16
	        ? (u32)1 << layer->nVS
	        : 0;
	const f32 layerWidth =
	    layer != NULL && layer->nHS >= 0 && layer->nHS < 16
	        ? (f32)(((u32)1 << layer->nHS) * (u32)layer->nTileWidth)
	        : 0.0f;
	const f32 layerHeight =
	    layer != NULL ? (f32)(height * (u32)layer->nTileHeight) : 0.0f;

	(void)sortBuffer;
	if (layer == NULL || layer->map == NULL || layer->tileDesc == NULL ||
	    layer->matDesc == NULL || layer->nHS < 0 || layer->nHS >= 16 ||
	    layer->nVS < 0 || layer->nVS >= 16 ||
	    (layer->nBPI != 1 && layer->nBPI != 2) ||
	    layer->nTileWidth <= 0 || layer->nTileHeight <= 0) {
		return;
	}

	for (material = 0; material < layer->nNumMaterials; ++material) {
		if (!G2DSetMaterialState(&layer->matDesc[material])) {
			continue;
		}
		for (copyY = layer->bWrap ? -1 : 0;
		     copyY <= (layer->bWrap ? 1 : 0); ++copyY) {
			for (copyX = layer->bWrap ? -1 : 0;
			     copyX <= (layer->bWrap ? 1 : 0); ++copyX) {
				for (y = 0; y < height; ++y) {
					G2DDrawMaterialRow(layer, material, y,
					                   (f32)copyX * layerWidth,
					                   (f32)copyY * layerHeight);
				}
			}
		}
	}
}

void G2DSetCamera(G2DPosOri* posOri)
{
	Mtx view;
	Vec position;
	Vec up;
	Vec target;
	f32 xOffset;
	f32 yOffset;

	if (posOri == NULL) {
		return;
	}
	g2dState.camera = *posOri;
	up.x = posOri->rOriX;
	up.y = posOri->rOriY;
	up.z = 0.0f;
	if (up.x == 0.0f && up.y == 0.0f) {
		up.y = 1.0f;
		g2dState.camera.rOriY = 1.0f;
	}

	xOffset = (f32)(((640 - g2dState.viewportWidth) >> 1) -
	               g2dState.viewportLeft);
	yOffset = (f32)(((448 - g2dState.viewportHeight) >> 1) -
	               g2dState.viewportTop);
	position.x = posOri->rPosX - up.x * yOffset - up.y * xOffset;
	position.y = posOri->rPosY + up.x * xOffset - up.y * yOffset;
	position.z = -300.0f;
	target.x = position.x;
	target.y = position.y;
	target.z = 0.0f;
	MTXLookAt(view, &position, &up, &target);
	GXLoadPosMtxImm(view, GX_PNMTX0);
}

void G2DInitWorld(u32 worldWidth, u32 worldHeight)
{
	Mtx44 projection;

	g2dState.worldWidth = (f32)worldWidth;
	g2dState.worldHeight = (f32)worldHeight;
	g2dState.halfWorldWidth = 0.5f * (f32)worldWidth;
	g2dState.halfWorldHeight = 0.5f * (f32)worldHeight;
	GXSetZMode(GX_FALSE, GX_ALWAYS, GX_TRUE);
	MTXOrtho(projection, 224.0f, -224.0f, -320.0f, 320.0f, 100.0f,
	         1000.0f);
	GXSetProjection(projection, GX_ORTHOGRAPHIC);
}

void G2DSetViewport(u16 left, u16 top, u16 width, u16 height)
{
	g2dState.viewportLeft = left;
	g2dState.viewportTop = top;
	g2dState.viewportWidth = width;
	g2dState.viewportHeight = height;
	GXSetScissor(left, top, width, height);
}
