/*---------------------------------------------------------------------------*
  Project:  Dolphin GD library
  File:     GDTev.c

  $Log: GDTev.c,v $
  Revision 1.2  2006/02/20 04:24:39  mitu
  Changed include path from dolphin/ to revolution/.

  Revision 1.1.1.1  2005/05/12 02:15:49  yasuh-to
  Ported from dolphin source tree.

    
    5     2001/11/07 6:44p Hirose
    Fixed GDSetTevColorS10 negative value bug.
    
    4     2001/09/28 4:37p Hirose
    Fixed GDSetTevColorS10 and GDSetTevKColor as well.
    
    3     2001/09/28 3:19a Hirose
    Fixed a bug in GDSetTevColor.
    
    2     2001/09/21 1:46p Carl
    Changed order of args for GDSetKonstantSel.
        
    
    1     2001/09/12 1:52p Carl
    Initial revision of GD: Graphics Display List Library.

  $NoKeywords: $
 *---------------------------------------------------------------------------*/

#include <revolution/gd.h>
#include <revolution/os.h>

/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
//  Name:         GDSetTevOp
//
//  Description:  Sets up various TEV configurations in a simplified way.
//                Note: the ras & tex swap parameters are set to TEV_SWAP0.
//
//  Arguments:    stage:  	which TEV stage will be set
//                mode:   	which simplified TEV mode
//                
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

void GDSetTevOp(GXTevStageID stage, GXTevMode mode)
{
    GXTevColorArg carg = GX_CC_RASC;
    GXTevAlphaArg aarg = GX_CA_RASA;

    if (stage != GX_TEVSTAGE0) {
        carg = GX_CC_CPREV;
        aarg = GX_CA_APREV;
    }

    switch (mode) {
      case GX_MODULATE:
        GDSetTevColorCalc( stage,
            GX_CC_ZERO, GX_CC_TEXC, carg, GX_CC_ZERO,
            GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV );
        GDSetTevAlphaCalcAndSwap( stage,
            GX_CA_ZERO, GX_CA_TEXA, aarg, GX_CA_ZERO,
            GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV, 
            GX_TEV_SWAP0, GX_TEV_SWAP0 );
        break;
      case GX_DECAL:
        GDSetTevColorCalc( stage,
            carg, GX_CC_TEXC, GX_CC_TEXA, GX_CC_ZERO,
            GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV );
        GDSetTevAlphaCalcAndSwap( stage,
            GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, aarg,
            GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV, 
            GX_TEV_SWAP0, GX_TEV_SWAP0 );
        break;
      case GX_BLEND:
        GDSetTevColorCalc( stage,
            carg, GX_CC_ONE, GX_CC_TEXC, GX_CC_ZERO,
            GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV );
        GDSetTevAlphaCalcAndSwap( stage,
            GX_CA_ZERO, GX_CA_TEXA, aarg, GX_CA_ZERO,
            GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV, 
            GX_TEV_SWAP0, GX_TEV_SWAP0 );
        break;
      case GX_REPLACE:
        GDSetTevColorCalc(stage,
            GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC,
            GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV );
        GDSetTevAlphaCalcAndSwap( stage,
            GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA,
            GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV, 
            GX_TEV_SWAP0, GX_TEV_SWAP0 );
        break;
      case GX_PASSCLR:
        GDSetTevColorCalc( stage,
            GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, carg,
            GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV );
        GDSetTevAlphaCalcAndSwap( stage,
            GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, aarg,
            GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV, 
            GX_TEV_SWAP0, GX_TEV_SWAP0 );
        break;
      default:
        ASSERTMSG(0, "GDSetTevOp: Invalid Tev Mode");
        break;
    }
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetTevColorCalc
//
//  Description:  Sets up the RGB part of a TEV calculation
//
//  Arguments:    stage:  	which TEV stage will be set
//                ...:    	parameters to set, as indicated by name
//                
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

void GDSetTevColorCalc(
    GXTevStageID    stage,
    GXTevColorArg   a,
    GXTevColorArg   b,
    GXTevColorArg   c,
    GXTevColorArg   d,
    GXTevOp         op,
    GXTevBias       bias,
    GXTevScale      scale,
    GXBool          clamp,
    GXTevRegID      out_reg )
{
    if (op <= GX_TEV_SUB) 
    {
        GDWriteBPCmd( TEV_COLOR_ENV( d, c, b, a, 
                                     bias, (op & 1), clamp, scale,
                                     out_reg,
                                     TEV_COLOR_ENV_0_ID + 2 * (u32) stage ));
    } else {
        GDWriteBPCmd( TEV_COLOR_ENV( d, c, b, a,
                                     GX_MAX_TEVBIAS, (op&1), clamp, ((op>>1)&3),
                                     out_reg,
                                     TEV_COLOR_ENV_0_ID + 2 * (u32) stage ));
    }
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetTevAlphaCalcAndSwap
//
//  Description:  Sets up the alpha part of a TEV calculation, as well as
//                selecting the raster and texture color swap modes
//
//  Arguments:    stage:  	which TEV stage will be set
//                ...:    	parameters to set, as indicated by name
//                
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

void GDSetTevAlphaCalcAndSwap(
    GXTevStageID    stage,
    GXTevAlphaArg   a,
    GXTevAlphaArg   b,
    GXTevAlphaArg   c,
    GXTevAlphaArg   d,
    GXTevOp         op,
    GXTevBias       bias,
    GXTevScale      scale,
    GXBool          clamp,
    GXTevRegID      out_reg,
    GXTevSwapSel    ras_sel,
    GXTevSwapSel    tex_sel )
{
    if (op <= GX_TEV_SUB) 
    {
        GDWriteBPCmd( TEV_ALPHA_ENV( ras_sel, tex_sel,
                                     d, c, b, a, 
                                     bias, (op & 1), clamp, scale,
                                     out_reg,
                                     TEV_ALPHA_ENV_0_ID + 2 * (u32) stage ));
    } else {
        GDWriteBPCmd( TEV_ALPHA_ENV( ras_sel, tex_sel,
                                     d, c, b, a,
                                     GX_MAX_TEVBIAS, (op&1), clamp, ((op>>1)&3),
                                     out_reg,
                                     TEV_ALPHA_ENV_0_ID + 2 * (u32) stage ));
    }
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetTevColor
//
//  Description:  Set one of the dynamic TEV color registers.
//                This versions lets you input RGBA8 colors.
//
//  Arguments:    reg:    	which dynamic TEV color register will be set
//                color:  	RGBA8 color value to write to register
//                
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

void GDSetTevColor   ( GXTevRegID reg, GXColor color )
{
    u32 regRA, regBG;

    regRA = TEV_REGISTERL( color.r, color.a, TEV_COLOR_REG, 
                           TEV_REGISTERL_0_ID + reg * 2 );
    regBG = TEV_REGISTERH( color.b, color.g, TEV_COLOR_REG, 
                           TEV_REGISTERH_0_ID + reg * 2 );
    GDWriteBPCmd( regRA );
    GDWriteBPCmd( regBG );
    // Due to color load delay bug, must put two more BP commands here...
    GDWriteBPCmd( regBG );
    GDWriteBPCmd( regBG );
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetTevColorS10
//
//  Description:  Set one of the dynamic TEV color registers.
//                This versions lets you input RGBA10 colors.
//
//  Arguments:    reg:    	which dynamic TEV color register will be set
//                color:  	RGBA8 color value to write to register
//                
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

#define TEV_REGISTER_VAL_SIZE   11
#define TEV_REGISTER_VAL_MASK   ((1<<TEV_REGISTER_VAL_SIZE)-1)

void GDSetTevColorS10( GXTevRegID reg, GXColorS10 color )
{
    u32 regRA, regBG;

    regRA = TEV_REGISTERL( (color.r & TEV_REGISTER_VAL_MASK),
                           (color.a & TEV_REGISTER_VAL_MASK),
                           TEV_COLOR_REG, 
                           TEV_REGISTERL_0_ID + reg * 2 );
    regBG = TEV_REGISTERH( (color.b & TEV_REGISTER_VAL_MASK),
                           (color.g & TEV_REGISTER_VAL_MASK),
                           TEV_COLOR_REG, 
                           TEV_REGISTERH_0_ID + reg * 2 );
    GDWriteBPCmd( regRA );
    GDWriteBPCmd( regBG );
    // Due to color load delay bug, must put two more BP commands here...
    GDWriteBPCmd( regBG );
    GDWriteBPCmd( regBG );
}

/*---------------------------------------------------------------------------*/
//  Name:         GXSetTevKColor
//
//  Description:  Sets one of four constant TEV color registers.
//
//  Arguments:    id:            	register name.
//                color:         	color value.
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

void GDSetTevKColor  ( GXTevKColorID reg, GXColor color )
{
    u32 regRA, regBG;

    regRA = TEV_REGISTERL( color.r, color.a, TEV_KONSTANT_REG,
                           TEV_REGISTERL_0_ID + reg * 2 );
    regBG = TEV_REGISTERH( color.b, color.g, TEV_KONSTANT_REG, 
                           TEV_REGISTERH_0_ID + reg * 2 );
    GDWriteBPCmd( regRA );
    GDWriteBPCmd( regBG );
    // The KColor registers do not have the load delay bug.
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetTevKColorSel
//
//  Description:  Selects form of Konstant color to use for a given pair
//                of TEV stages.
//
//  Arguments:    evenStage:    	tev stage id (must be even)
//                kcsel0:       	selection for Konstant color for evenStage
//                kasel0:       	selection for Konstant alpha for evenStage
//                kcsel1:       	selection for Konstant color for evenStage+1
//                kasel1:       	selection for Konstant alpha for evenStage+1
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

#define TEV_KSEL_MASK_KCSEL \
    (( 0x00001F << TEV_KSEL_KCSEL0_SHIFT ) | \
     ( 0x00001F << TEV_KSEL_KASEL0_SHIFT ) | \
     ( 0x00001F << TEV_KSEL_KCSEL1_SHIFT ) | \
     ( 0x00001F << TEV_KSEL_KASEL1_SHIFT ))

void GDSetTevKonstantSel( GXTevStageID evenStage, 
                          GXTevKColorSel kcsel0, GXTevKAlphaSel kasel0, 
                          GXTevKColorSel kcsel1, GXTevKAlphaSel kasel1 )
{
    // Mask to avoid touching the swap-select tables
    GDWriteBPCmd( SS_MASK( TEV_KSEL_MASK_KCSEL ) );
    GDWriteBPCmd( TEV_KSEL( 0, 0, kcsel0, kasel0, kcsel1, kasel1,
                            TEV_KSEL_0_ID + (evenStage/2) ));
}

/*---------------------------------------------------------------------------*/
//  Name:         GXSetTevSwapModeTable
//
//  Description:  Sets the table of 4 different swap mode possibilities.
//
//  Arguments:    table:         	which swap table to set
//
//                red, green:	    which channel to output for each input
//                blue, alpha
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

#define TEV_KSEL_MASK_SWAPMODETABLE \
    (( 0x000003 << TEV_KSEL_XRB_SHIFT ) | \
     ( 0x000003 << TEV_KSEL_XGA_SHIFT ))

void GDSetTevSwapModeTable(
    GXTevSwapSel    table,
    GXTevColorChan  red,
    GXTevColorChan  green,
    GXTevColorChan  blue,
    GXTevColorChan  alpha )
{
    // Mask to avoid touching the konstant-color selects
    GDWriteBPCmd( SS_MASK( TEV_KSEL_MASK_SWAPMODETABLE ) );
    GDWriteBPCmd( TEV_KSEL( red, green, 0, 0, 0, 0,
                            TEV_KSEL_0_ID + (table * 2) ));
    GDWriteBPCmd( SS_MASK( TEV_KSEL_MASK_SWAPMODETABLE ) );
    GDWriteBPCmd( TEV_KSEL( blue, alpha, 0, 0, 0, 0,
                            TEV_KSEL_0_ID + (table * 2) + 1 ));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetAlphaCompare  
//
//  Description:  Sets alpha value comparison function and parameters.
//
//  Arguments:    comp0:       	compare function 0.
//                ref0:        	reference value 0.
//                op:          	logical op to combine the two comparison results.
//                comp1:       	compare function 0.
//                ref1:        	reference value 1.
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

void GDSetAlphaCompare(
    GXCompare       comp0,
    u8              ref0,
    GXAlphaOp       op,
    GXCompare       comp1,
    u8              ref1 )
{
    GDWriteBPCmd( TEV_ALPHAFUNC( ref0, ref1, comp0, comp1, op,
                                 TEV_ALPHAFUNC_ID ));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetZTexture 
//
//  Description:  Sets parameters for controlling z textures in the last
//                tev stage.
//
//  Arguments:    op:    	Z Texture operation 
//                fmt:   	Format of z texels.
//                bias:  	bias to add to the z values.
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

void GDSetZTexture ( GXZTexOp op, GXTexFmt fmt, u32 bias )
{
    u32 zfmt;

    switch(fmt) {
      case GX_TF_Z8:    zfmt = TEV_Z_TYPE_U8;   break;
      case GX_TF_Z16:   zfmt = TEV_Z_TYPE_U16;  break;
      case GX_TF_Z24X8: zfmt = TEV_Z_TYPE_U24;  break;
      default:
        ASSERTMSG(0, "GDSetZTexture: Invalid format");       
        zfmt = TEV_Z_TYPE_U24; break;
    }

    GDWriteBPCmd( TEV_Z_ENV_0( bias, TEV_Z_ENV_0_ID ));
    GDWriteBPCmd( TEV_Z_ENV_1( zfmt, op, TEV_Z_ENV_1_ID ));
}

/*---------------------------------------------------------------------------*/
//  Name:         GXSetTevOrder 
//
//  Description:  Sets texture map id, texture coordinate id, and color id
//                for the given pair of texture stages.
//
//  Arguments:    evenStage:    	tev stage id (must be even)
//                ...:          	parameters to set, as indicated by name
//                
//  Returns:      
//
/*---------------------------------------------------------------------------*/

void GDSetTevOrder(
    GXTevStageID    evenStage, 
    GXTexCoordID    coord0,
    GXTexMapID      map0,
    GXChannelID     color0,
    GXTexCoordID    coord1,
    GXTexMapID      map1,
    GXChannelID     color1 )
{
    static u8 c2r[] = {         // Convert enums to HW values
        RAS1_CC_0,              // GX_COLOR0
        RAS1_CC_1,              // GX_COLOR1
        RAS1_CC_0,              // GX_ALPHA0
        RAS1_CC_1,              // GX_ALPHA1
        RAS1_CC_0,              // GX_COLOR0A0
        RAS1_CC_1,              // GX_COLOR1A1
        RAS1_CC_Z,              // GX_COLOR_ZERO
        RAS1_CC_B,              // GX_COLOR_BUMP
        RAS1_CC_BN,             // GX_COLOR_BUMPN
        0,                      // 9
        0,                      // 10
        0,                      // 11
        0,                      // 12
        0,                      // 13
        0,                      // 14
        RAS1_CC_Z               // 15: GX_COLOR_NULL gets mapped here
    };

    GDWriteBPCmd( RAS1_TREF(
        (map0 & 7),                                             // map 0
        (coord0 & 7),                                           // tc 0
        ((map0 != GX_TEXMAP_NULL) && !(map0 & GX_TEX_DISABLE)), // enable 0
        c2r[ color0 & 0xf ],                                    // color 0
        (map1 & 7),                                             // map 1
        (coord1 & 7),                                           // tc 1
        ((map1 != GX_TEXMAP_NULL) && !(map1 & GX_TEX_DISABLE)), // enable 1
        c2r[ color1 & 0xf ],                                    // color 1
        (RAS1_TREF0_ID + (evenStage/2)) ));
}


