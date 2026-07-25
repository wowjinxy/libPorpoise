/*---------------------------------------------------------------------------*
  Project:  Dolphin GD library
  File:     GDTexture.c

  $Log: GDTexture.c,v $
  Revision 1.3  2006/02/20 04:24:39  mitu
  Changed include path from dolphin/ to revolution/.

  Revision 1.2  2006/02/03 08:54:38  hirose
  Avoided use of EPPC and use WIN32.

  Revision 1.1.1.1  2005/05/12 02:15:49  yasuh-to
  Ported from dolphin source tree.

    
    7     2001/10/30 2:11p Hirose
    Fixed time stamp problem.
    
    7     2002/10/17 4:37p Hirose
    Fixed image type bit setting of GDSetTexPreLoaded().
    
    6     2001/10/13 2:27a Hirose
    Changed GDSetTexCoordScale function.
    
    5     2001/10/10 12:06p Carl
    Fixed filter conversion table.
    
    4     2001/10/04 4:46p Carl
    Added GDLoadTlut, etc.
    
    3     10/02/01 5:16p Carl
    Fixed bug in GDSetTexTlut.
    
    2     2001/09/14 6:43p Carl
    Made OSCachedToPhysical a macro for non-EPPC.
    
    1     2001/09/12 1:52p Carl
    Initial revision of GD: Graphics Display List Library.

  $NoKeywords: $
 *---------------------------------------------------------------------------*/

#include <revolution/gd.h>
#include <revolution/os.h>

#if defined(WIN32) && !defined(OSCachedToPhysical)
#define OSCachedToPhysical(caddr) ((u32)((u8*)(caddr) - (0x80000000)))
#endif

/*---------------------------------------------------------------------------*/

// The table below is convenient for using the GX default TLUT allocation.
// It defines TMEM addresses for 20 TLUT's:
// - the first 16 TLUT's are 256 entry
// - the final  4 TLUT's are 1024 entry

#ifdef DEFINE_TLUT_TABLE
#define GX_TMEM_HI  0x80000
#define GX_32k      0x08000
#define GX_8k       0x02000
u32 GXTlutRegions[20] = {
    GX_TMEM_HI + GX_32k*8 + GX_8k*0,
    GX_TMEM_HI + GX_32k*8 + GX_8k*1,
    GX_TMEM_HI + GX_32k*8 + GX_8k*2,
    GX_TMEM_HI + GX_32k*8 + GX_8k*3,
    GX_TMEM_HI + GX_32k*8 + GX_8k*4,
    GX_TMEM_HI + GX_32k*8 + GX_8k*5,
    GX_TMEM_HI + GX_32k*8 + GX_8k*6,
    GX_TMEM_HI + GX_32k*8 + GX_8k*7,
    GX_TMEM_HI + GX_32k*8 + GX_8k*8,
    GX_TMEM_HI + GX_32k*8 + GX_8k*9,
    GX_TMEM_HI + GX_32k*8 + GX_8k*10,
    GX_TMEM_HI + GX_32k*8 + GX_8k*11,
    GX_TMEM_HI + GX_32k*8 + GX_8k*12,
    GX_TMEM_HI + GX_32k*8 + GX_8k*13,
    GX_TMEM_HI + GX_32k*8 + GX_8k*14,
    GX_TMEM_HI + GX_32k*8 + GX_8k*15,
    GX_TMEM_HI + GX_32k*8 + GX_8k*16 + GX_32k*0,
    GX_TMEM_HI + GX_32k*8 + GX_8k*16 + GX_32k*1,
    GX_TMEM_HI + GX_32k*8 + GX_8k*16 + GX_32k*2,
    GX_TMEM_HI + GX_32k*8 + GX_8k*16 + GX_32k*3,
};
#endif

// The tables below are used in the library code to map texmap ID's
// to register addresses.  If you are linking with GX, you can use
// the same tables that are defined within GX.

#ifndef LINKING_WITH_GX

u8 GDTexMode0Ids[8] = {
    TX_SETMODE0_I0_ID,
    TX_SETMODE0_I1_ID,
    TX_SETMODE0_I2_ID,
    TX_SETMODE0_I3_ID,
    TX_SETMODE0_I4_ID,
    TX_SETMODE0_I5_ID,
    TX_SETMODE0_I6_ID,
    TX_SETMODE0_I7_ID,
};

u8 GDTexMode1Ids[8] = {
    TX_SETMODE1_I0_ID,
    TX_SETMODE1_I1_ID,
    TX_SETMODE1_I2_ID,
    TX_SETMODE1_I3_ID,
    TX_SETMODE1_I4_ID,
    TX_SETMODE1_I5_ID,
    TX_SETMODE1_I6_ID,
    TX_SETMODE1_I7_ID,
};

u8 GDTexImage0Ids[8] = {
    TX_SETIMAGE0_I0_ID,
    TX_SETIMAGE0_I1_ID,
    TX_SETIMAGE0_I2_ID,
    TX_SETIMAGE0_I3_ID,
    TX_SETIMAGE0_I4_ID,
    TX_SETIMAGE0_I5_ID,
    TX_SETIMAGE0_I6_ID,
    TX_SETIMAGE0_I7_ID,
};

u8 GDTexImage1Ids[8] = {
    TX_SETIMAGE1_I0_ID,
    TX_SETIMAGE1_I1_ID,
    TX_SETIMAGE1_I2_ID,
    TX_SETIMAGE1_I3_ID,
    TX_SETIMAGE1_I4_ID,
    TX_SETIMAGE1_I5_ID,
    TX_SETIMAGE1_I6_ID,
    TX_SETIMAGE1_I7_ID,
};

u8 GDTexImage2Ids[8] = {
    TX_SETIMAGE2_I0_ID,
    TX_SETIMAGE2_I1_ID,
    TX_SETIMAGE2_I2_ID,
    TX_SETIMAGE2_I3_ID,
    TX_SETIMAGE2_I4_ID,
    TX_SETIMAGE2_I5_ID,
    TX_SETIMAGE2_I6_ID,
    TX_SETIMAGE2_I7_ID,
};

u8 GDTexImage3Ids[8] = {
    TX_SETIMAGE3_I0_ID,
    TX_SETIMAGE3_I1_ID,
    TX_SETIMAGE3_I2_ID,
    TX_SETIMAGE3_I3_ID,
    TX_SETIMAGE3_I4_ID,
    TX_SETIMAGE3_I5_ID,
    TX_SETIMAGE3_I6_ID,
    TX_SETIMAGE3_I7_ID,
};

u8 GDTexTlutIds[8] = {
    TX_SETTLUT_I0_ID,
    TX_SETTLUT_I1_ID,
    TX_SETTLUT_I2_ID,
    TX_SETTLUT_I3_ID,
    TX_SETTLUT_I4_ID,
    TX_SETTLUT_I5_ID,
    TX_SETTLUT_I6_ID,
    TX_SETTLUT_I7_ID,
};

u8 GD2HWFiltConv[] = {
    0, // TX_MIN_NEAREST,                     // GX_NEAR,
    4, // TX_MIN_LINEAR,                      // GX_LINEAR,
    1, // TX_MIN_NEAREST_MIPMAP_NEAREST,      // GX_NEAR_MIP_NEAR,
    5, // TX_MIN_LINEAR_MIPMAP_NEAREST,       // GX_LIN_MIP_NEAR,
    2, // TX_MIN_NEAREST_MIPMAP_LINEAR,       // GX_NEAR_MIP_LIN,
    6, // TX_MIN_LINEAR_MIPMAP_LINEAR,        // GX_LIN_MIP_LIN
};

#else

// If we're linking with GX, no need to duplicate these arrays; just use GX's values.

#define GDTexMode0Ids  GXTexMode0Ids
#define GDTexMode1Ids  GXTexMode1Ids
#define GDTexImage0Ids GXTexImage0Ids
#define GDTexImage1Ids GXTexImage1Ids
#define GDTexImage2Ids GXTexImage2Ids
#define GDTexImage3Ids GXTexImage3Ids
#define GDTexTlutIds   GXTexTlutIds
#define GD2HWFiltConv  GX2HWFiltConv

#endif

/*---------------------------------------------------------------------------*/
//  Name:         GDSetTexLookupMode
//
//  Description:  Sets various texture lookup attributes for a given tex ID.
//
//  Arguments:    id:    	which texture ID will be set
//                ...:   	parameters to set, as indicated by name
//                
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

void GDSetTexLookupMode ( GXTexMapID id,
                          GXTexWrapMode wrap_s, 
                          GXTexWrapMode wrap_t,
                          GXTexFilter min_filt, 
                          GXTexFilter mag_filt, 
                          f32 min_lod, 
                          f32 max_lod,
                          f32 lod_bias,
                          GXBool bias_clamp,
                          GXBool do_edge_lod, 
                          GXAnisotropy max_aniso )
{
    GDWriteBPCmd(TX_SETMODE0(wrap_s,
                             wrap_t,
                             (mag_filt == GX_LINEAR),
                             GD2HWFiltConv[min_filt],
                             !do_edge_lod,
                             (u8)((s8)(lod_bias*32.0f)),
                             max_aniso,
                             bias_clamp,
                             GDTexMode0Ids[id]));
    GDWriteBPCmd(TX_SETMODE1((u8)(min_lod*16.0f),
                             (u8)(max_lod*16.0f),
                             GDTexMode1Ids[id]));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetTexImgAttr
//
//  Description:  Sets image attributes for a given tex ID.
//
//  Arguments:    id:    	which texture ID will be set
//                ...:   	parameters to set, as indicated by name
//                
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

void GDSetTexImgAttr ( GXTexMapID id, u16 width, u16 height, GXTexFmt format )
{
    GDWriteBPCmd(TX_SETIMAGE0(width - 1,
                              height - 1,
                              format,
                              GDTexImage0Ids[id]));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetTexImgPtr
//
//  Description:  Sets main-memory address for a given tex ID.
//
//  Arguments:    id:        	which texture ID will be set
//                image_ptr:	 (cached) main-memory address of texture
//                
//  Returns:      	none
//
/*---------------------------------------------------------------------------*/

void GDSetTexImgPtr ( GXTexMapID id, void *image_ptr )
{
    GDWriteBPCmd(TX_SETIMAGE3(OSCachedToPhysical(image_ptr)>>5,
                              GDTexImage3Ids[id]));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetTexImgPtrRaw
//
//  Description:  Sets main-memory address for a given tex ID.
//                Doesn't do any modification of the ptr provided.
//
//  Arguments:    id:             	which texture ID will be set
//                image_ptr_raw:  	a u24 parameter to be inserted where the
//                               HW-format address would otherwise go.
//                
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

void GDSetTexImgPtrRaw ( GXTexMapID id, u32 image_ptr_raw )
{
    GDWriteBPCmd(TX_SETIMAGE3(image_ptr_raw, GDTexImage3Ids[id]));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDPatchTexImgPtr
//
//  Description:  Alters texture memory address for a GDSetTexImgAddr command.
//                We assume that the DL offset has been moved to the point
//                BP_DATA_OFFSET bytes after where the GDSetTexImgAddr command
//                was inserted.  Only the texture address is changed.
//
//  Arguments:    image_ptr:	     (cached) main-memory address of texture
//                
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

void GDPatchTexImgPtr ( void *image_ptr )
{
    // Note: offset MUST be pointing to the 1st byte of register data

    GDWrite_u24(OSCachedToPhysical(image_ptr)>>5); // write texture address
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetTexCached
//
//  Description:  Indicates that a texture is cached; sets cache parameters.
//
//  Arguments:    id:    	which texture ID will be set
//                ...:   	parameters to set, as indicated by name
//                
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

void GDSetTexCached ( GXTexMapID id,
                      u32 tmem_even, GXTexCacheSize size_even,
                      u32 tmem_odd,  GXTexCacheSize size_odd )
{
    GDWriteBPCmd(TX_SETIMAGE1( tmem_even >> 5,
                               size_even+3, 
                               size_even+3, 
                               0, 
                               GDTexImage1Ids[id]));
    if (size_odd != GX_TEXCACHE_NONE && tmem_odd < GX_TMEM_MAX)
    {
        GDWriteBPCmd(TX_SETIMAGE2( tmem_odd >> 5,
                                   size_odd+3,
                                   size_odd+3,
                                   GDTexImage2Ids[id]));
    }
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetTexPreLoaded
//
//  Description:  Indicates that a texture is preloaded; sets TMEM parameters.
//
//  Arguments:    id:    	which texture ID will be set
//                ...:   	parameters to set, as indicated by name
//                
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

void GDSetTexPreLoaded ( GXTexMapID id, u32 tmem_even, u32 tmem_odd )
{
    GDWriteBPCmd(TX_SETIMAGE1( tmem_even >> 5,
                               0,
                               0,
                               1, 
                               GDTexImage1Ids[id]));
    if (tmem_odd < GX_TMEM_MAX) // need to define this!
    {
        GDWriteBPCmd(TX_SETIMAGE2( tmem_odd >> 5,
                                   0,
                                   0,
                                   GDTexImage2Ids[id]));
    }
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetTexTlut
//
//  Description:  Sets TLUT parameters for a given tex ID.
//
//  Arguments:    id:    	which texture ID will be set
//                ...:   	parameters to set, as indicated by name
//                
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

void GDSetTexTlut ( GXTexMapID id, u32 tmem_addr, GXTlutFmt format )
{
    GDWriteBPCmd(TX_SETTLUT( (tmem_addr-GX_TMEM_HALF) >> 9, format,
                             GDTexTlutIds[id]));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetTexCoordScale
//
//  Description:  Sets texture scale parameters for a given texcoord (not tex!)
//                ID.  Uses the register mask to avoid changing the texture
//                offset enable bits.
//
//  Arguments:    coord:     	which texcoord ID will be set
//                ...:       	parameters to set, as indicated by name
//                
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

#define SU_TS0_MASK_SETTEXCOORDSCALE \
    ( 0x00FFFF << SU_TS0_SSIZE_SHIFT )
#define SU_TS1_MASK_SETTEXCOORDSCALE \
    ( 0x00FFFF << SU_TS1_TSIZE_SHIFT )

void GDSetTexCoordScale ( GXTexCoordID coord, u16 s_scale, u16 t_scale )
{
    // mask prevents write to bias, wrap and point/line offset enables
    GDWriteBPCmd(SS_MASK(SU_TS0_MASK_SETTEXCOORDSCALE));
    GDWriteBPCmd(SU_TS0( s_scale - 1, 0, 0, 0, 0,
                         SU_SSIZE0_ID + (u32) coord * 2));

    GDWriteBPCmd(SS_MASK(SU_TS1_MASK_SETTEXCOORDSCALE));
    GDWriteBPCmd(SU_TS1( t_scale - 1, 0, 0,
                         SU_TSIZE0_ID + (u32) coord * 2));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetTexCoordScale2
//
//  Description:  Sets texture scale parameters for a given texcoord (not tex!)
//                ID. Besides, this API sets texcoord bias and cycle wrap mode
//                as well.
//
//  Arguments:    coord:     	which texcoord ID will be set
//                ...:       	parameters to set, as indicated by name
//                
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

#define SU_TS0_MASK_SETTEXCOORDSCALE2 \
    (( 0x00FFFF << SU_TS0_SSIZE_SHIFT ) | \
     ( 0x000001 << SU_TS0_BS_SHIFT )    | \
     ( 0x000001 << SU_TS0_WS_SHIFT ))

void GDSetTexCoordScale2 ( GXTexCoordID coord,
                           u16 s_scale, GXBool s_bias, GXBool s_wrap,
                           u16 t_scale, GXBool t_bias, GXBool t_wrap)
{
    // mask prevents write to point & line offset enables
    GDWriteBPCmd(SS_MASK(SU_TS0_MASK_SETTEXCOORDSCALE2));
    GDWriteBPCmd(SU_TS0( s_scale - 1, s_bias, s_wrap, 0, 0,
                         SU_SSIZE0_ID + (u32) coord * 2));
    GDWriteBPCmd(SU_TS1( t_scale - 1, t_bias, t_wrap,
                         SU_TSIZE0_ID + (u32) coord * 2));
}


/*---------------------------------------------------------------------------*/
//  Name:         GDSetTexCoordScaleandTOEs
//
//  Description:  Sets texture scale parameters for a given texcoord (not tex!)
//                ID.  Also sets the texture offset enable bits (TOEs).
//
//  Arguments:    coord:     	which texcoord ID will be set
//                ...:       	parameters to set, as indicated by name
//                
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

void GDSetTexCoordScaleAndTOEs ( GXTexCoordID coord,
                                 u16 s_scale, GXBool s_bias, GXBool s_wrap,
                                 u16 t_scale, GXBool t_bias, GXBool t_wrap,
                                 GXBool line_offset, GXBool point_offset)
{
    GDWriteBPCmd(SU_TS0( s_scale - 1, s_bias, s_wrap, line_offset, point_offset,
                         SU_SSIZE0_ID + (u32) coord * 2));
    GDWriteBPCmd(SU_TS1( t_scale - 1, t_bias, t_wrap,
                         SU_TSIZE0_ID + (u32) coord * 2));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDLoadTlut
//
//  Description:  Loads a TLUT from main memory into TMEM.
//
//  Arguments:    tlut_ptr  : (cached) main-memory address of TLUT
//                tmem_addr : destination address of TLUT in TMEM
//                size      : size of the TLUT
//                
//  Returns:      none
//
//  Notes:        For patching, you must save the following offset:
//                GDGetCurrOffset() + BP_CMD_LENGTH * 2 + BP_DATA_OFFSET
//
/*---------------------------------------------------------------------------*/

void GDLoadTlut( void *tlut_ptr, u32 tmem_addr, GXTlutSize size )
{
    ASSERTMSG((tmem_addr & 0x1ff)==0, "GDLoadTlut: invalid TMEM pointer");
    ASSERTMSG((size <= 1024), "GDLoadTlut: invalid TLUT size");

    // Flush the texture state (without modifying the indirect mask)
    GDWriteBPCmd(SS_MASK(0xffff00));
    GDWriteBPCmd((BU_IMASK_ID << 24));

    GDWriteBPCmd(TX_LOADTLUT0( OSCachedToPhysical(tlut_ptr)>>5,
                             TX_LOADTLUT0_ID));

    // Writing to this register will initiate the actual loading:
    GDWriteBPCmd(TX_LOADTLUT1( (tmem_addr-GX_TMEM_HALF) >> 9, size,
                             TX_LOADTLUT1_ID));

    // Flush the texture state (without modifying the indirect mask)
    GDWriteBPCmd(SS_MASK(0xffff00));
    GDWriteBPCmd((BU_IMASK_ID << 24));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDLoadTlutRaw
//
//  Description:  Loads a TLUT from main memory into TMEM.
//                Doesn't do any modification of the ptr provided.
//
//  Arguments:    tlut_ptr_raw : a u24 parameter to be inserted where the
//                               HW-format address would otherwise go.
//                tmem_addr    : destination address of TLUT in TMEM
//                size         : size of the TLUT
//                
//  Returns:      none
//
//  Notes:        For patching, you must save the following offset:
//                GDGetCurrOffset() + BP_CMD_LENGTH * 2 + BP_DATA_OFFSET
//
/*---------------------------------------------------------------------------*/

void GDLoadTlutRaw( u32 tlut_ptr_raw, u32 tmem_addr, GXTlutSize size )
{
    ASSERTMSG((size <= 1024), "GDLoadTlut: invalid TLUT size");

    // Flush the texture state (without modifying the indirect mask)
    GDWriteBPCmd(SS_MASK(0xffff00));
    GDWriteBPCmd((BU_IMASK_ID << 24));

    GDWriteBPCmd(TX_LOADTLUT0( tlut_ptr_raw, TX_LOADTLUT0_ID));

    // Writing to this register will initiate the actual loading:
    GDWriteBPCmd(TX_LOADTLUT1( (tmem_addr-GX_TMEM_HALF) >> 9, size,
                             TX_LOADTLUT1_ID));

    // Flush the texture state (without modifying the indirect mask)
    GDWriteBPCmd(SS_MASK(0xffff00));
    GDWriteBPCmd((BU_IMASK_ID << 24));
}

