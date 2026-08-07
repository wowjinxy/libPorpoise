/*---------------------------------------------------------------------------*
  Project: Texture Test
  File:    txtestdata.h

  Copyright 1998, 1999 Nintendo.  All rights reserved.

  These coded instructions, statements, and computer programs contain
  proprietary information of Nintendo of America Inc. and/or Nintendo
  Company Ltd., and are protected by Federal copyright law.  They may
  not be disclosed to third parties or copied or duplicated in any form,
  in whole or in part, without the prior written consent of Nintendo.

  Change History:
   $Log: /Dolphin/build/demos/gxdemo/include/tex-fmt-cube.h $
    
    1     3/06/00 12:03p Alligator
    move from gx/tests to demos/gxdemos and rename
    
    7     8/29/99 9:40p Hirose
    changed some data
    
    6     7/30/99 6:13p Hirose
    
    5     7/29/99 6:14p Hirose
    Changed to fit to new format set
    
    4     7/14/99 5:57p Hirose
    
    3     7/13/99 8:31p Hirose
    
    2     7/13/99 1:20p Hirose

    1     7/07/99 4:33p Hirose

   $NoKeywords: $
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <dolphin/gx.h>

/*---------------------------------------------------------------------------*/
#define TEST_I4_TEX_IMAGE_WIDTH		32
#define TEST_I4_TEX_IMAGE_HEIGHT	32
#define TEST_I4_TEX_IMAGE_FORMAT	GX_TF_I4

extern u8 TestI4TexImageData[];
/*---------------------------------------------------------------------------*/
#define TEST_I8_TEX_IMAGE_WIDTH		32
#define TEST_I8_TEX_IMAGE_HEIGHT	32
#define TEST_I8_TEX_IMAGE_FORMAT	GX_TF_I8

extern u8 TestI8TexImageData[];
/*---------------------------------------------------------------------------*/
#define TEST_IA4_TEX_IMAGE_WIDTH	32
#define TEST_IA4_TEX_IMAGE_HEIGHT	32
#define TEST_IA4_TEX_IMAGE_FORMAT	GX_TF_IA4

extern u8 TestIA4TexImageData[];
/*---------------------------------------------------------------------------*/
#define TEST_IA8_TEX_IMAGE_WIDTH	16
#define TEST_IA8_TEX_IMAGE_HEIGHT	16
#define TEST_IA8_TEX_IMAGE_FORMAT	GX_TF_IA8

extern u8 TestIA8TexImageData[];
/*---------------------------------------------------------------------------*/
#define TEST_RGB565_TEX_IMAGE_WIDTH		16
#define TEST_RGB565_TEX_IMAGE_HEIGHT	16
#define TEST_RGB565_TEX_IMAGE_FORMAT	GX_TF_RGB565

extern u8 TestRGB565TexImageData[];
/*---------------------------------------------------------------------------*/
#define TEST_RGB5A3_TEX_IMAGE_WIDTH		64
#define TEST_RGB5A3_TEX_IMAGE_HEIGHT	64
#define TEST_RGB5A3_TEX_IMAGE_FORMAT	GX_TF_RGB5A3

extern u8 TestRGB5A3TexImageData[];
/*---------------------------------------------------------------------------*/
#define TEST_RGBA8_TEX_IMAGE_WIDTH		8
#define TEST_RGBA8_TEX_IMAGE_HEIGHT		8
#define TEST_RGBA8_TEX_IMAGE_FORMAT		GX_TF_RGBA8

extern u8 TestRGBA8TexImageData[];
/*---------------------------------------------------------------------------*/
#define TEST_C4_TEX_IMAGE_WIDTH			8
#define TEST_C4_TEX_IMAGE_HEIGHT		8
#define TEST_C4_TEX_IMAGE_FORMAT		GX_TF_C4

extern u8 TestC4TexImageData[];
/*---------------------------------------------------------------------------*/
#define TEST_C8_TEX_IMAGE_WIDTH			8
#define TEST_C8_TEX_IMAGE_HEIGHT		8
#define TEST_C8_TEX_IMAGE_FORMAT		GX_TF_C8

extern u8 TestC8TexImageData[];
/*---------------------------------------------------------------------------*/
/*
#define TEST_CA4_TEX_IMAGE_WIDTH		8
#define TEST_CA4_TEX_IMAGE_HEIGHT		8
#define TEST_CA4_TEX_IMAGE_FORMAT		GX_TF_CA4

extern u8 TestCA4TexImageData[];
*/
/*---------------------------------------------------------------------------*/
/*
#define TEST_C6A2_TEX_IMAGE_WIDTH		8
#define TEST_C6A2_TEX_IMAGE_HEIGHT		8
#define TEST_C6A2_TEX_IMAGE_FORMAT		GX_TF_C6A2

extern u8 TestC6A2TexImageData[];
*/
/*---------------------------------------------------------------------------*/
/*
#define TEST_CA8_TEX_IMAGE_WIDTH		16
#define TEST_CA8_TEX_IMAGE_HEIGHT		16
#define TEST_CA8_TEX_IMAGE_FORMAT		GX_TF_CA8

extern u8 TestCA8TexImageData[];
*/
/*---------------------------------------------------------------------------*/
/*
#define TEST_C12A4_TEX_IMAGE_WIDTH		8
#define TEST_C12A4_TEX_IMAGE_HEIGHT		8
#define TEST_C12A4_TEX_IMAGE_FORMAT		GX_TF_C12A4

extern u16 TestC12A4TexImageData[];
*/
/*---------------------------------------------------------------------------*/
#define TEST_C14X2_TEX_IMAGE_WIDTH		8
#define TEST_C14X2_TEX_IMAGE_HEIGHT		8
#define TEST_C14X2_TEX_IMAGE_FORMAT		GX_TF_C14X2

extern u16 TestC14X2TexImageData[];
/*---------------------------------------------------------------------------*/
#define TEST_CMPR_TEX_IMAGE_WIDTH		16
#define TEST_CMPR_TEX_IMAGE_HEIGHT		16
#define TEST_CMPR_TEX_IMAGE_FORMAT		GX_TF_CMPR

extern u16 TestCMPRTexImageData[];
/*---------------------------------------------------------------------------*/
#define TEST_MIPMAP_TEX_IMAGE_WIDTH		32
#define TEST_MIPMAP_TEX_IMAGE_HEIGHT	32
#define TEST_MIPMAP_TEX_IMAGE_FORMAT	GX_TF_RGB5A3
#define TEST_MIPMAP_TEX_IMAGE_MIN_LOD	0.0
#define TEST_MIPMAP_TEX_IMAGE_MAX_LOD	4.0
#define TEST_MIPMAP_TEX_IMAGE_LOD_BIAS	0.0

extern u16 TestMipmapTexImageData[];
/*---------------------------------------------------------------------------*/
#define TEST_TLUT00_FORMAT		GX_TL_RGB5A3

extern u16 TestTlutData00[];
/*---------------------------------------------------------------------------*/
#define TEST_TLUT01_FORMAT		GX_TL_RGB5A3

extern u16 TestTlutData01[];
/*---------------------------------------------------------------------------*/
#define TEST_TLUT02_FORMAT		GX_TL_RGB565

extern u16 TestTlutData02[];
/*---------------------------------------------------------------------------*/
#define TEST_TLUT03_FORMAT		GX_TL_RGB565

extern u16 TestTlutData03[];
/*---------------------------------------------------------------------------*/
