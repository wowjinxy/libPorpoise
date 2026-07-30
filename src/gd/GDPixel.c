/*---------------------------------------------------------------------------*
  Project:  Dolphin GD library
  File:     GDPixel.c

  $Log: GDPixel.c,v $
  Revision 1.2  2006/02/20 04:24:39  mitu
  Changed include path from dolphin/ to revolution/.

  Revision 1.1.1.1  2005/05/12 02:15:49  yasuh-to
  Ported from dolphin source tree.

    
    4     2003/07/17 11:42 Hirose
    Fixed "PE_CMODE1_MASK_SETBLENDMODE" macro definition.
    
    3     2002/10/28 11:08 Hirose
    Supported fog functions for orthographic projection.
    
    2     2001/10/13 2:27a Hirose
    Added GDSetBlendMode().
    
    1     2001/09/12 1:52p Carl
    Initial revision of GD: Graphics Display List Library.
  $NoKeywords: $
 *---------------------------------------------------------------------------*/

#include <revolution/gd.h>
#include <revolution/os.h>

/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
//  Name:         GDSetFog  
//
//  Description:  Computes and sets fog parameters.
//
//  Arguments:    type:    	IN Fog type
//                startz,
//                endz:    	IN Z Range for linear fog.
//                nearz,
//                farz:    	IN Clipping planes Z values (projection matrix)
//                color:   	IN Color of the fog.
//
//  Returns:      
//
/*---------------------------------------------------------------------------*/

void GDSetFog( GXFogType    type,
               f32          startz,
               f32          endz,
               f32          nearz,
               f32          farz,
               GXColor      color )
{
    f32     A, B, B_mant, C, A_f;
    u32     b_expn, b_m, a_hex, c_hex;
    u32     fsel, proj;

    ASSERTMSG(farz >= 0, "GDSetFog: The farz should be positive value");
    ASSERTMSG(farz >= nearz, "GDSetFog: The farz should be larger than nearz");

    fsel = (u32)(type & 0x07);
    proj = (u32)((type >> 3) & 0x01);

    if ( proj ) // ORTHOGRAPHIC
    {
        // Calculate constants a and c (TEV HW requirements).
        if ((farz == nearz) || (endz == startz))
        {
            // take care of the odd-ball case.
            A_f = 0.0f;
            C   = 0.0f;
        }
        else
        {
            A   = 1.0F / (endz - startz);
            A_f = (farz - nearz) * A;
            C   = (startz - nearz) * A;
        }

        b_expn = 0;
        b_m    = 0;
    }
    else // PERSPECTIVE
    {
        // Calculate constants a, b, and c (TEV HW requirements).
        if ((farz == nearz) || (endz == startz))
        {
            // take care of the odd-ball case.
            A = 0.0f;
            B = 0.5f;
            C = 0.0f;
        }
        else
        {
            A = (farz * nearz) / ((farz-nearz) * (endz-startz));
            B = farz / (farz-nearz);
            C = startz / (endz-startz);
        }
        
        B_mant = B;
        b_expn = 1;
        while (B_mant > 1.0) 
        {
            B_mant /= 2;
            b_expn++;
        }
        while ((B_mant > 0) && (B_mant < 0.5))
        {
            B_mant *= 2;
            b_expn--;
        }

        A_f   = A / (1 << (b_expn));
        b_m   = (u32)(B_mant * 8388638);
    }

    a_hex = (* (u32 *) &A_f);
    c_hex = (* (u32 *) &C);

    // Write out register values.
    GDWriteBPCmd( TEV_FOG_PARAM_0_PS( (a_hex >> 12), TEV_FOG_PARAM_0_ID ));

    GDWriteBPCmd( TEV_FOG_PARAM_1( b_m, TEV_FOG_PARAM_1_ID ));
    GDWriteBPCmd( TEV_FOG_PARAM_2( b_expn, TEV_FOG_PARAM_2_ID ));

    GDWriteBPCmd( TEV_FOG_PARAM_3_PS( (c_hex >> 12), proj, fsel, 
                                      TEV_FOG_PARAM_3_ID ));

    GDWriteBPCmd( TEV_FOG_COLOR( color.b, color.g, color.r, TEV_FOG_COLOR_ID ));
}


/*---------------------------------------------------------------------------*/
//  Name:         GDSetBlendMode
//
//  Description:  Sets Blending and logic operation registers in PE.
//                Register mask is used to prevent changing other status
//                in the cmode0 register.
//
//  Arguments:    type:          	Blending or Logic op enable.
//                src_factor:    	Blending source factor.
//                dst_factor:    	Blending destination factor.
//                logic_op:      	Logic operation.
//
//  Returns:      None.
//
/*---------------------------------------------------------------------------*/

#define PE_CMODE1_MASK_SETBLENDMODE \
    (( 0x000001 << PE_CMODE0_BLEND_ENABLE_SHIFT )   | \
     ( 0x000001 << PE_CMODE0_LOGICOP_ENABLE_SHIFT ) | \
     ( 0x000007 << PE_CMODE0_DFACTOR_SHIFT )        | \
     ( 0x000007 << PE_CMODE0_SFACTOR_SHIFT )        | \
     ( 0x000001 << PE_CMODE0_BLENDOP_SHIFT )        | \
     ( 0x00000F << PE_CMODE0_LOGICOP_SHIFT ))


void GDSetBlendMode( GXBlendMode    type,
                     GXBlendFactor  src_factor, 
                     GXBlendFactor  dst_factor, 
                     GXLogicOp      logic_op )
{
    GDWriteBPCmd( SS_MASK( PE_CMODE1_MASK_SETBLENDMODE ));

    GDWriteBPCmd( PE_CMODE0( 
        ((type == GX_BM_BLEND) || (type == GX_BM_SUBTRACT)),
        (type == GX_BM_LOGIC),
        0,
        0,
        0,
        dst_factor,
        src_factor,
        (type == GX_BM_SUBTRACT),
        logic_op,
        PE_CMODE0_ID ));
}


/*---------------------------------------------------------------------------*/
//  Name:         GDSetBlendModeEtc
//
//  Description:  Sets Blending and logic operation registers in PE.
//                Also sets color and alpha write masks and dither mode.
//
//  Arguments:    type:          	Blending or Logic op enable.
//                src_factor:    	Blending source factor.
//                dst_factor:    	Blending destination factor.
//                logic_op:      	Logic operation.
//                color_update_enable:   	as it says
//                alpha_update_enable:   	as it says
//                dither_enable:         	as it says
//
//  Returns:      None.
//
/*---------------------------------------------------------------------------*/

void GDSetBlendModeEtc( GXBlendMode     type,
                        GXBlendFactor   src_factor, 
                        GXBlendFactor   dst_factor, 
                        GXLogicOp       logic_op,
                        GXBool          color_update_enable,
                        GXBool          alpha_update_enable,
                        GXBool          dither_enable )
{
    GDWriteBPCmd( PE_CMODE0( 
        ((type == GX_BM_BLEND) || (type == GX_BM_SUBTRACT)),
        (type == GX_BM_LOGIC),
        dither_enable,
        color_update_enable,
        alpha_update_enable,
        dst_factor,
        src_factor,
        (type == GX_BM_SUBTRACT),
        logic_op,
        PE_CMODE0_ID ));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetZMode
//
//  Description:  Sets zbuffer compare and update parameters.
//
//  Arguments:    compare_enable:  	enables z comparison and culling.
//                func:            	function used in z comparision.
//                update_enable:   	enables writing new z values to zbuffer.
//
//  Returns:      None.
//
/*---------------------------------------------------------------------------*/

void GDSetZMode( GXBool     compare_enable,
                 GXCompare  func,
                 GXBool     update_enable )
{
    GDWriteBPCmd( PE_ZMODE( compare_enable, func, update_enable, PE_ZMODE_ID ));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetDstAlpha
//
//  Description:  Sets a constant alpha value for writing to framebuffer.
//
//  Arguments:    enable:        	Enable constant alpha writes.
//                alpha:         	constant value.
//
//  Returns:      None.
//
/*---------------------------------------------------------------------------*/

void GDSetDstAlpha( GXBool enable, u8 alpha )
{
    GDWriteBPCmd( PE_CMODE1( alpha, enable, PE_CMODE1_ID ));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetDrawSync  
//
//  Description:  Writes a sync token to the PE's token register via GFX fifo. 
//
//  Arguments:    token:         	token value to be written to the pipe.
//                
//  Returns:      
//
/*---------------------------------------------------------------------------*/

void GDSetDrawSync( u16 token )
{
    // TOKEN_INT is the register that is compared against TOKEN
    // to see whether or not to generate a CPU interrupt.
    // TOKEN is the register that is readable using GXReadDrawSync().

    GDWriteBPCmd( PE_TOKEN( token, PE_TOKEN_INT_ID ));
    GDWriteBPCmd( PE_TOKEN( token, PE_TOKEN_ID ));
}


