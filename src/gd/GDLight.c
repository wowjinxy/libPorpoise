/*---------------------------------------------------------------------------*
  Project:  Dolphin GD library
  File:     GDLight.c

  $Log: GDLight.c,v $
  Revision 1.6  2007/11/22 13:13:23  iwai_yuma
  (none)

  Revision 1.5  2007/11/22 13:12:27  iwai_yuma
  (none)

  Revision 1.4  2007/11/21 19:23:14  iwai
  Added #pragma warning(disable: 4005).

  Revision 1.3  2006/02/20 04:24:39  mitu
  Changed include path from dolphin/ to revolution/.

  Revision 1.2  2005/08/17 10:55:09  hirose
  Moved definition of sinf/cosf for WIN32 from gd.h

  Revision 1.1.1.1  2005/05/12 02:15:49  yasuh-to
  Ported from dolphin sheath tree.

    
    4    2002/12/18 9:59 Hirose
    Changed definition of BIG_NUMBER.
    
    3    2002/12/18 9:41 Hirose
    Added singularity check in GDSetSpecularDir( ).
    
    2     2001/09/17 9:33p Hirose
    Changes regarding specular light APIs.
    
    1     2001/09/12 1:52p Carl
    Initial revision of GD: Graphics Display List Library.
  $NoKeywords: $
 *---------------------------------------------------------------------------*/

#include <revolution/gd.h>
#include <revolution/os.h>
#include <math.h>

#pragma warning(disable: 4005)

/*---------------------------------------------------------------------------*/

#ifdef WIN32
#define cosf   (float)cos
#define sqrtf  (float)sqrt
#endif

/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
//  Name:         GDSetLightAttn  
//
//  Description:  Sets light attenuation parameters directly.
//
//  Arguments:    light         HW light ID.
//                a0,...        Attenuation parameters.
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

void GDSetLightAttn     ( GXLightID light,
                          f32 a0, f32 a1, f32 a2,
                          f32 k0, f32 k1, f32 k2 )
{
    GDWriteXFCmdHdr( (u16) (XF_LIGHT0A0_ID + __GDLightID2Offset(light)), 6 );
    GDWrite_f32(a0);
    GDWrite_f32(a1);
    GDWrite_f32(a2);
    GDWrite_f32(k0);
    GDWrite_f32(k1);
    GDWrite_f32(k2);
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetLightSpot 
//
//  Description:  Convenience function to set spotlight parameters.
//
//  Arguments:    light         HW light ID.
//                cutoff        Cut off angle.
//                spot_func     Spot function characteristics.
//  Returns:      
//
/*---------------------------------------------------------------------------*/

#define  PI    3.14159265358979323846F

void GDSetLightSpot( GXLightID light, f32 cutoff, GXSpotFn spot_func )
{
    f32 a0, a1, a2, r, d, cr;

    if ( cutoff <= 0.0F || cutoff > 90.0F ) {
        spot_func = GX_SP_OFF;
    }

    r = cutoff * PI / 180.0F;
    cr = cosf(r);

    switch ( spot_func ) {
        case GX_SP_FLAT :
            a0 = - 1000.0F * cr;
            a1 = 1000.0F;
            a2 = 0.0F;
            break;
        case GX_SP_COS :
            a0 = - cr / ( 1.0F - cr );
            a1 = 1.0F / ( 1.0F - cr );
            a2 = 0.0F;
            break;
        case GX_SP_COS2 :
            a0 = 0.0F;
            a1 = - cr / ( 1.0F - cr );
            a2 = 1.0F / ( 1.0F - cr );
            break;
        case GX_SP_SHARP :
            d  = ( 1.0F - cr ) * ( 1.0F - cr );
            a0 = cr * ( cr - 2.0F ) / d;
            a1 = 2.0F / d;
            a2 = -1.0F / d;
            break;
        case GX_SP_RING1 :
            d  = ( 1.0F - cr ) * ( 1.0F - cr );
            a0 = -4.0F * cr / d;
            a1 = 4.0F * ( 1.0F + cr ) / d;
            a2 = -4.0F / d;
            break;
        case GX_SP_RING2 :
            d  = ( 1.0F - cr ) * ( 1.0F - cr );
            a0 = 1.0F - 2.0F * cr * cr / d;
            a1 = 4.0F * cr / d;
            a2 = -2.0F / d;
            break;
        case GX_SP_OFF :
        default :
            a0 = 1.0F;
            a1 = 0.0F;
            a2 = 0.0F;
            break;
    }

    GDWriteXFCmdHdr( (u16) (XF_LIGHT0A0_ID + __GDLightID2Offset(light)), 3 );
    GDWrite_f32(a0);
    GDWrite_f32(a1);
    GDWrite_f32(a2);
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetLightDistAttn
//
//  Description:  Convenience function for setting distance attenuation.
//
//  Arguments:    light         HW light ID.
//                ref_dist      Reference distance.
//                ref_br        Reference brightness.
//                dist_func     Attenuation characteristics.
//  Returns:      
//
/*---------------------------------------------------------------------------*/

void GDSetLightDistAttn( GXLightID light, f32 ref_dist, f32 ref_br, 
                          GXDistAttnFn dist_func )
{
    f32 k0, k1, k2;

    if ( ref_dist < 0.0F ||
         ref_br  <= 0.0F || ref_br >= 1.0F ) {
        dist_func = GX_DA_OFF;
    }

    switch ( dist_func ) {
        case GX_DA_GENTLE :
            k0 = 1.0F;
            k1 = ( 1.0F - ref_br ) / ( ref_br * ref_dist );
            k2 = 0.0F;
            break;
        case GX_DA_MEDIUM :
            k0 = 1.0F;
            k1 = 0.5F * ( 1.0f - ref_br ) / ( ref_br * ref_dist );
            k2 = 0.5F * ( 1.0f - ref_br ) / ( ref_br * ref_dist * ref_dist );
            break;
        case GX_DA_STEEP :
            k0 = 1.0F;
            k1 = 0.0F;
            k2 = ( 1.0F - ref_br ) / ( ref_br * ref_dist * ref_dist );
            break;
        case GX_DA_OFF :
        default :
            k0 = 1.0F;
            k1 = 0.0F;
            k2 = 0.0F;
            break;
    }
  
    GDWriteXFCmdHdr( (u16) (XF_LIGHT0K0_ID + __GDLightID2Offset(light)), 3 );
    GDWrite_f32(k0);
    GDWrite_f32(k1);
    GDWrite_f32(k2);
}

/*---------------------------------------------------------------------------*/
// Name:    GDSetLightColor
//
// Desc:    Sets the Diffuse Color of the light directly.
//
// Args:    light:      HW light ID.
//          color:      Diffuse color.
//
/*---------------------------------------------------------------------------*/

void GDSetLightColor 	( GXLightID light, GXColor color )
{
    GDWriteXFCmd( (u16) (XF_LIGHT0COLOR_ID + __GDLightID2Offset(light)),
                  (((u32)color.r << 24) | 
                   ((u32)color.g << 16) |
                   ((u32)color.b <<  8) | 
                   ((u32)color.a)) );
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetLightPos
//
//  Description:  Sets the position of light.
//
//  Arguments:    light:   HW light ID.
//                x, y, z:   Light position/vector.
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

void GDSetLightPos 	( GXLightID light, f32 x, f32 y, f32 z )
{
    GDWriteXFCmdHdr( (u16) (XF_LIGHT0PX_ID + __GDLightID2Offset(light)), 3 );
    GDWrite_f32(x);
    GDWrite_f32(y);
    GDWrite_f32(z);
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetLightDir
//
//  Description:  Sets the direction of (spot) light.
//
//  Arguments:    light:      HW light ID.
//                nx,ny,nz:   Light direction vector.
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

void GDSetLightDir 	( GXLightID light, f32 nx, f32 ny, f32 nz )
{
    GDWriteXFCmdHdr( (u16) (XF_LIGHT0DX_ID + __GDLightID2Offset(light)), 3 );
    GDWrite_f32(nx);
    GDWrite_f32(ny);
    GDWrite_f32(nz);
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetSpecularDirHA
//
//  Description:  Sets the direction and half-angle vector of specular light.
//
//  Arguments:    light:      HW light ID.
//                nx,ny,nz:   Light direction vector.
//                hx,hy,hz:   Half-angle vector.
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

#define BIG_NUMBER      1.0E+18F

void GDSetSpecularDirHA ( GXLightID light, f32 nx, f32 ny, f32 nz, f32 hx, f32 hy, f32 hz )
{
    f32 px, py, pz;

    px = -nx * BIG_NUMBER;
    py = -ny * BIG_NUMBER;
    pz = -nz * BIG_NUMBER;

    GDWriteXFCmdHdr( (u16) (XF_LIGHT0PX_ID + __GDLightID2Offset(light)), 6 );
    GDWrite_f32(px);
    GDWrite_f32(py);
    GDWrite_f32(pz);
    GDWrite_f32(hx);
    GDWrite_f32(hy);
    GDWrite_f32(hz);
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetSpecularDir
//
//  Description:  Sets the direction and half-angle vector of specular light.
//                In this function, half-angle is calculated automatically.
//
//  Arguments:    light:      HW light ID.
//                nx,ny,nz:   Light direction vector.
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

void GDSetSpecularDir ( GXLightID light, f32 nx, f32 ny, f32 nz )
{
    f32 px, py, pz;
    f32 hx, hy, hz, mag;

    // Compute half-angle vector
    hx  = -nx;
    hy  = -ny;
    hz  = (-nz + 1.0f);
    mag = hx * hx + hy * hy + hz * hz;

    if ( mag != 0.0f ) // Singularity check
    {
        mag = 1.0f / sqrtf(mag);
    }

    hx *= mag;
    hy *= mag;
    hz *= mag;

    px  = -nx * BIG_NUMBER;
    py  = -ny * BIG_NUMBER;
    pz  = -nz * BIG_NUMBER;

    GDWriteXFCmdHdr( (u16) (XF_LIGHT0PX_ID + __GDLightID2Offset(light)), 6 );
    GDWrite_f32(px);
    GDWrite_f32(py);
    GDWrite_f32(pz);
    GDWrite_f32(hx);
    GDWrite_f32(hy);
    GDWrite_f32(hz);
}

#undef BIG_NUMBER

/*---------------------------------------------------------------------------*/
// Name:    GDLoadLightObjIndx
//
// Desc:    Loads lighting registers in xf block using array index
//
// Args:    lt_obj_indx         Index of light object in object array.
//          light         HW light ID.
//
/*---------------------------------------------------------------------------*/

void GDLoadLightObjIndx	( u32 lt_obj_indx, GXLightID light )
{
    GDWriteXFIndxDCmd( (u16) (XF_LIGHT_BASE_ID + __GDLightID2Offset(light)),
                       XF_LIGHT_SIZE, (u16) lt_obj_indx );
}

/*---------------------------------------------------------------------------*/
// 
// Name:    GDSetChanAmbColor
//
// Desc:    Sets Global Ambient Color in xform block
//
// Args:    chan:       Color name 
//          amb_color:  Ambient color
//
/*---------------------------------------------------------------------------*/

void GDSetChanAmbColor 	( GXChannelID chan, GXColor color )
{
    GDWriteXFCmd( (u16) (XF_AMBIENT0_ID + (chan & 1)),
                  (((u32)color.r << 24) | 
                   ((u32)color.g << 16) |
                   ((u32)color.b <<  8) | 
                   ((u32)color.a)) );
}

/*---------------------------------------------------------------------------*/
// 
// Name:    GDSetChanMatColor
//
// Desc:    Sets Global Material Color in xform block
//
// Args:    chan:       Color name 
//          amb_color:  Material color
//
/*---------------------------------------------------------------------------*/

void GDSetChanMatColor	( GXChannelID chan, GXColor color )
{
    GDWriteXFCmd( (u16) (XF_MATERIAL0_ID + (chan & 1)),
                  (((u32)color.r << 24) | 
                   ((u32)color.g << 16) |
                   ((u32)color.b <<  8) | 
                   ((u32)color.a)) );
}

/*---------------------------------------------------------------------------*/
// 
// Name:    GDSetChanCtrl
//
// Desc:    Sets lighting controls for a given lighting channel.
//
// Args:    1. chan:        IN Channel ID.
//          2. enable:      IN Enable lighting for this channel.
//          3. amb_src:     IN Source for ambient color.
//          4. mat_src:     IN Source for material color.
//          5. light_mask:  IN Lights used in the lighting equation.
//          6. diff_fn:     IN Diffuse function for lighting.
//          7. attn_fn:     IN Attenuation function name.
//
/*---------------------------------------------------------------------------*/

void GDSetChanCtrl      ( GXChannelID   chan,
                          GXBool        enable,
                          GXColorSrc    amb_src,
                          GXColorSrc    mat_src,
                          u32           light_mask,
                          GXDiffuseFn   diff_fn,
                          GXAttnFn      attn_fn )
{
    u32 reg;

    reg =  XF_COLOR0CNTRL_F_PS( mat_src, enable, (light_mask & 0x0f), amb_src,
                                ((attn_fn==GX_AF_SPEC) ? GX_DF_NONE : diff_fn),
                                (attn_fn != GX_AF_NONE),
                                (attn_fn != GX_AF_SPEC),
                                ((light_mask >> 4) & 0x0f) );
    GDWriteXFCmd( (u16) (XF_COLOR0CNTRL_ID + (chan & 3)), reg );

    if (chan == GX_COLOR0A0 || chan == GX_COLOR1A1)
    {
        GDWriteXFCmd( (u16) (XF_COLOR0CNTRL_ID + ( chan - 2)), reg );
    }
}


