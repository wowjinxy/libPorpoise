/*---------------------------------------------------------------------------*
  Project:  Dolphin GD library
  File:     GDIndirect.c

  $Log: GDIndirect.c,v $
  Revision 1.2  2006/02/20 04:24:39  mitu
  Changed include path from dolphin/ to revolution/.

  Revision 1.1.1.1  2005/05/12 02:15:49  yasuh-to
  Ported from dolphin source tree.

    
    2     2003/02/19 17:37 Hirose
    
    1     2003/02/03 14:03 Hirose
    Initial check in.
    
  $NoKeywords: $
 *---------------------------------------------------------------------------*/

#include <revolution/gd.h>
#include <revolution/os.h>

/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*
    Macros
 *---------------------------------------------------------------------------*/
#define IND_TEX_MTX_EXP     10
#define IND_TEX_MTX_SCALE   (f32)(1 << IND_TEX_MTX_EXP)
#define IND_TEX_MTX_MASK    0x7ff
#define IND_TEX_SCALE_BIAS  0x11


/*---------------------------------------------------------------------------*/
//  Name:         GDSetTevIndirect
//
//  Desc:         Compile the state of an Indirect texture object in to HW 
//                dependent values.
//
//  Arguments:    tev_stage:  	TEV stage name.
//                ind_stage:  	Index of the Indirect texture being bound       
//                format:     	format of indirect texture offsets.
//                bias_sel:   	Bias added to the texture offsets.
//                matrix_sel: 	Selects texture offset matrix.
//                wrap_s:     	Wrap value of Direct S coordinate.
//                wrap_t:     	Wrap value of Direct T coordinate
//                add_prev:   	Add output from previous stage to texture coords.
//                utc_lod:    	Use the unmodified texture coordinates for LOD.
//                alpha_sel:  	Selects indirect texture alpha output.
//                
//  Returns:      none
//
/*---------------------------------------------------------------------------*/
void GDSetTevIndirect (
    GXTevStageID        tev_stage,
    GXIndTexStageID     ind_stage,
    GXIndTexFormat      format,
    GXIndTexBiasSel     bias_sel,
    GXIndTexMtxID       matrix_sel,
    GXIndTexWrap        wrap_s,
    GXIndTexWrap        wrap_t,
    GXBool              add_prev,
    GXBool              utc_lod,
    GXIndTexAlphaSel    alpha_sel )
{
    GDWriteBPCmd( IND_CMD(
        (ind_stage & 3),
        (format & 3),
        (bias_sel & 7),
        (alpha_sel & 3),
        (matrix_sel & 0x0F),
        (wrap_s & 7),
        (wrap_t & 7),
        (utc_lod & 1),
        (add_prev & 1),
        (IND_CMD0_ID + (tev_stage & 0x0F)) ));
}

/*---------------------------------------------------------------------------*/
// 
// Name:          GDSetIndTexMtx
//
// Desc:          Sets transformation matrix for indirect texture coordinates.
//
// Arguments:     MtxId:                Matrix Name.
//                OffsetMatrix[3][2]:   Matrix for the texture offsets.
//                ScaleExp:             Exponent value for scale ( scale = 2ScaleExp)
//
// Returns:       none
//
/*---------------------------------------------------------------------------*/
void GDSetIndTexMtx ( GXIndTexMtxID mtx_id, const f32 offset[2][3], s8 scale_exp )
{
    u32     id_offset;
    
    switch ( mtx_id )
    {
      case GX_ITM_0:
        id_offset = 0;
        break;
      case GX_ITM_1:
        id_offset = 3;
        break;
      case GX_ITM_2:
        id_offset = 6;
        break;
      default:
        ASSERTMSG(0, "GDSetIndTexMtx: Invalid matrix id");
        break;
    }

    scale_exp += IND_TEX_SCALE_BIAS;

    GDWriteBPCmd( IND_MTXA(
        (((s32)(offset[0][0] * IND_TEX_MTX_SCALE)) & IND_TEX_MTX_MASK),
        (((s32)(offset[1][0] * IND_TEX_MTX_SCALE)) & IND_TEX_MTX_MASK),
        (scale_exp & 0x03),
        (IND_MTXA0_ID + id_offset) ));
    
    GDWriteBPCmd( IND_MTXB(
        (((s32)(offset[0][1] * IND_TEX_MTX_SCALE)) & IND_TEX_MTX_MASK),
        (((s32)(offset[1][1] * IND_TEX_MTX_SCALE)) & IND_TEX_MTX_MASK),
        ((scale_exp >> 2) & 0x03),
        (IND_MTXB0_ID + id_offset) ));

    GDWriteBPCmd( IND_MTXC(
        (((s32)(offset[0][2] * IND_TEX_MTX_SCALE)) & IND_TEX_MTX_MASK),
        (((s32)(offset[1][2] * IND_TEX_MTX_SCALE)) & IND_TEX_MTX_MASK),
        ((scale_exp >> 4) & 0x03),
        (IND_MTXC0_ID + id_offset) ));
}

/*---------------------------------------------------------------------------*/
// 
// Name:          GDSetIndTexScale
//
// Desc:          Sets scale value for indirect textures.
//
// Args:          indStageEven: 	Indirect stage id (must be even).
//                scaleS0:       	Scale value for S (even stage).
//                scaleT0:       	Scale value for T (even stage).
//                scaleS1:       	Scale value for S (odd stage).
//                scaleT1:       	Scale value for T (odd stage).
//
/*---------------------------------------------------------------------------*/
void GDSetIndTexCoordScale (
    GXIndTexStageID     indStageEven,
    GXIndTexScale       scaleS0,
    GXIndTexScale       scaleT0,
    GXIndTexScale       scaleS1,
    GXIndTexScale       scaleT1 )
{
    GDWriteBPCmd( RAS1_SS(
        (scaleS0 & 0x0F),
        (scaleT0 & 0x0F),
        (scaleS1 & 0x0F),
        (scaleT1 & 0x0F),
        (RAS1_SS0_ID + ((indStageEven >> 1) & 0x01)) ));
}

/*---------------------------------------------------------------------------*/
// 
// Name:          GDSetIndTexOrder
//
// Desc:          Associates direct texture maps and coordinate names
//                with indirect texture maps.
//
// Arguments:     texCoord0:    	Associated texcoord for indirect texture 0.
//                texMap0:      	Associated texture map for indirect texture 0.
//                texCoord1:    	Associated texcoord for indirect texture 1.
//                texMap1:      	Associated texture map for indirect texture 1.
//                texCoord2:    	Associated texcoord for indirect texture 2.
//                texMap2:      	Associated texture map for indirect texture 2.
//                texCoord3:    	Associated texcoord for indirect texture 3.
//                texMap3:      	Associated texture map for indirect texture 3.
//
/*---------------------------------------------------------------------------*/
void GDSetIndTexOrder (
    GXTexCoordID        texCoord0,
    GXTexMapID          texMap0, 
    GXTexCoordID        texCoord1,
    GXTexMapID          texMap1, 
    GXTexCoordID        texCoord2,
    GXTexMapID          texMap2, 
    GXTexCoordID        texCoord3,
    GXTexMapID          texMap3 )
{
    GDWriteBPCmd( RAS1_IREF(
        (texMap0 & 7),
        (texCoord0 & 7),
        (texMap1 & 7),
        (texCoord1 & 7),
        (texMap2 & 7),
        (texCoord2 & 7),
        (texMap3 & 7),
        (texCoord3 & 7),
        RAS1_IREF_ID) );
}

/*---------------------------------------------------------------------------*/
// 
// Name:        GDSetTevDirect
//
// Desc:        Sets a TEV stage back to a normal, direct lookup.
//          
// Arguments:   tev_stage: 	TEV stage number.
//
/*---------------------------------------------------------------------------*/
void GDSetTevDirect ( GXTevStageID tev_stage )
{
    GDSetTevIndirect(tev_stage, GX_INDTEXSTAGE0, GX_ITF_8,
                     GX_ITB_NONE, GX_ITM_OFF, GX_ITW_OFF, GX_ITW_OFF,
                     GX_FALSE, GX_FALSE, GX_ITBA_OFF);
}

/*---------------------------------------------------------------------------*/
// 
// Name:    GDSetTevIndWarp
//
// Desc:    Used for simple indirect texture warping.
//
/*---------------------------------------------------------------------------*/
void GDSetTevIndWarp ( GXTevStageID tev_stage, GXIndTexStageID ind_stage,
                       GXBool signed_offset, GXBool replace_mode,
                       GXIndTexMtxID matrix_sel)
{
    GXIndTexWrap wrap = (replace_mode) ? GX_ITW_0 : GX_ITW_OFF;

    GDSetTevIndirect(tev_stage,         // tev stage
                     ind_stage,         // indirect stage
                     GX_ITF_8,          // format
                     (signed_offset) ? GX_ITB_STU : GX_ITB_NONE, // bias
                     matrix_sel,        // matrix select
                     wrap,              // wrap direct S
                     wrap,              // wrap direct T
                     GX_FALSE,          // add prev stage output?
                     GX_FALSE,          // use unmodified TC for LOD?
                     GX_ITBA_OFF );     // bump alpha select
}

/*---------------------------------------------------------------------------*/
// 
// Name:    GXSetTevIndTile
//
// Desc:    Used for indirect texture tiling and pseudo-3D texturing.
//
/*---------------------------------------------------------------------------*/
void GDSetTevIndTile ( GXTevStageID tev_stage, GXIndTexStageID ind_stage,
                       u16 tilesize_s, u16 tilesize_t, 
                       u16 tilespacing_s, u16 tilespacing_t, 
                       GXIndTexFormat format, GXIndTexMtxID matrix_sel,
                       GXIndTexBiasSel bias_sel, GXIndTexAlphaSel alpha_sel)
{
    GXIndTexWrap        wrap_s, wrap_t;
    f32                 mtx[2][3];

    // Compute wrap parameters from tilesize
    switch (tilesize_s) {
        case 256:       wrap_s = GX_ITW_256;    break;  
        case 128:       wrap_s = GX_ITW_128;    break;  
        case 64:        wrap_s = GX_ITW_64;     break;  
        case 32:        wrap_s = GX_ITW_32;     break;  
        case 16:        wrap_s = GX_ITW_16;     break;  
        default:
            ASSERTMSG(0, "GDSetTevIndTile: Invalid tilesize for S coordinate");
            wrap_s = GX_ITW_OFF;
            break;
    }

    switch (tilesize_t) {
        case 256:       wrap_t = GX_ITW_256;    break;  
        case 128:       wrap_t = GX_ITW_128;    break;  
        case 64:        wrap_t = GX_ITW_64;     break;  
        case 32:        wrap_t = GX_ITW_32;     break;  
        case 16:        wrap_t = GX_ITW_16;     break;  
        default:
            ASSERTMSG(0, "GDSetTevIndTile: Invalid tilesize for T coordinate");
            wrap_t = GX_ITW_OFF;
            break;
    }

    // compute the matrix using tilespacing values.
    mtx[0][0] = ((f32) tilespacing_s / IND_TEX_MTX_SCALE);
    mtx[0][1] = mtx[0][2] = 0.0f;
    mtx[1][1] = ((f32) tilespacing_t / IND_TEX_MTX_SCALE);
    mtx[1][0] = mtx[1][2] = 0.0f;
    GDSetIndTexMtx(matrix_sel, mtx, IND_TEX_MTX_EXP); 
    
    GDSetTevIndirect(tev_stage,         // tev stage
                     ind_stage,         // indirect stage
                     format,            // format
                     bias_sel,          // bias select
                     matrix_sel,        // matrix select
                     wrap_s,            // wrap direct S
                     wrap_t,            // wrap direct T
                     GX_FALSE,          // add prev stage output?
                     GX_TRUE,           // use unmodified TC for LOD?
                     alpha_sel);        // bump alpha select
}

/*---------------------------------------------------------------------------*/
// 
// Name:    GDSetTevIndBumpST
//
// Desc:    Set up TEV stages for environment-mapped bump-mapped texture
//          lookup.  The bump map is in ST space.
//
/*---------------------------------------------------------------------------*/
void GDSetTevIndBumpST ( GXTevStageID tev_stage, GXIndTexStageID ind_stage, 
                         GXIndTexMtxID matrix_sel )
{
    GXIndTexMtxID sm, tm;

    switch(matrix_sel) 
    {
      case GX_ITM_0:
        sm = GX_ITM_S0;
        tm = GX_ITM_T0;
        break;
      case GX_ITM_1:
        sm = GX_ITM_S1;
        tm = GX_ITM_T1;
        break;
      case GX_ITM_2:
        sm = GX_ITM_S2;
        tm = GX_ITM_T2;
        break;
      default:
        ASSERTMSG(0, "GDSetTevIndBumpST: Invalid matrix selection");
    }

    GDSetTevIndirect(tev_stage,         // tev stage
                     ind_stage,         // indirect stage
                     GX_ITF_8,          // format
                     GX_ITB_ST,         // bias
                     sm,                // matrix select
                     GX_ITW_0,          // wrap direct S
                     GX_ITW_0,          // wrap direct T
                     GX_FALSE,          // add prev stage output?
                     GX_FALSE,          // use unmodified TC for LOD?
                     GX_ITBA_OFF);      // bump alpha select

    GDSetTevIndirect((GXTevStageID) (tev_stage+1),       // tev stage
                     ind_stage,         // indirect stage
                     GX_ITF_8,          // format
                     GX_ITB_ST,         // bias
                     tm,                // matrix select
                     GX_ITW_0,          // wrap direct S
                     GX_ITW_0,          // wrap direct T
                     GX_TRUE,           // add prev stage output?
                     GX_FALSE,          // use unmodified TC for LOD?
                     GX_ITBA_OFF);      // bump alpha select

    GDSetTevIndirect((GXTevStageID) (tev_stage+2),       // tev stage
                     ind_stage,         // indirect stage
                     GX_ITF_8,          // format
                     GX_ITB_NONE,       // bias
                     GX_ITM_OFF,        // matrix select
                     GX_ITW_OFF,        // wrap direct S
                     GX_ITW_OFF,        // wrap direct T
                     GX_TRUE,           // add prev stage output?
                     GX_FALSE,          // use unmodified TC for LOD?
                     GX_ITBA_OFF);      // bump alpha select
}

/*---------------------------------------------------------------------------*/
// 
// Name:    GDSetTevIndBumpXYZ
//
// Desc:    Set up TEV stages for environment-mapped bump-mapped texture
//          lookup.  The bump map is in 3D object space.
//
/*---------------------------------------------------------------------------*/
void GDSetTevIndBumpXYZ ( GXTevStageID tev_stage, GXIndTexStageID ind_stage,
                          GXIndTexMtxID matrix_sel )
{
    GDSetTevIndirect(tev_stage,         // tev stage
                     ind_stage,         // indirect stage
                     GX_ITF_8,          // format
                     GX_ITB_STU,        // bias
                     matrix_sel,        // matrix select
                     GX_ITW_OFF,        // wrap direct S
                     GX_ITW_OFF,        // wrap direct T
                     GX_FALSE,          // add prev stage output?
                     GX_FALSE,          // use unmodified TC for LOD?
                     GX_ITBA_OFF);      // bump alpha select
}

/*---------------------------------------------------------------------------*/
// 
// Name:    GDSetTevIndRepeat
//
// Desc:    Provides a texture coordinate that is the repeat of the
//          TC computation from the previous stage.  Intended only to
//          be used with GDSetTevIndBumpST().
//
/*---------------------------------------------------------------------------*/
void GDSetTevIndRepeat ( GXTevStageID tev_stage )
{
    GDSetTevIndirect(tev_stage,         // tev stage
                     GX_INDTEXSTAGE0,   // indirect stage
                     GX_ITF_8,          // format
                     GX_ITB_NONE,       // bias
                     GX_ITM_OFF,        // matrix select
                     GX_ITW_0,          // wrap direct S
                     GX_ITW_0,          // wrap direct T
                     GX_TRUE,           // add prev stage output?
                     GX_FALSE,          // use unmodified TC for LOD?
                     GX_ITBA_OFF);      // bump alpha select
}

/*---------------------------------------------------------------------------*/
// 
// Name:          __GDSetIndTexMask
//
// Desc:          Sets new value of imask register.
//                (Probably not necessary to do, though)
//
// Arguments:     mask:     	Mask parameter.
//
/*---------------------------------------------------------------------------*/
void __GDSetIndTexMask ( u32 mask )
{
    GDWriteBPCmd( IND_IMASK(
        (mask & 0x0FF),
        IND_IMASK_ID) );
}

/*---------------------------------------------------------------------------*/

