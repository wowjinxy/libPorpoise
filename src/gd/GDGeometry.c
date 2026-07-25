/*---------------------------------------------------------------------------*
  Project:  Dolphin/Revolution GD library
  File:     GDGeometry.c

  $Log: GDGeometry.c,v $
  Revision 1.4  2007/05/24 17:33:21  carlmu
  Fixed ASSERT issue in GDSetVtxAttrFmtv.

  Revision 1.3  2006/02/20 04:24:39  mitu
  Changed include path from dolphin/ to revolution/.

  Revision 1.2  2006/02/03 08:54:44  hirose
  Avoided use of EPPC and use WIN32.

  Revision 1.1.1.1  2005/05/12 02:15:49  yasuh-to
  Ported from dolphin source tree.

    
    7     2002/08/05 19:53 Hirose
    Const type specifier support.
    
    6     2001/10/13 2:25a Hirose
    Added GDSetCullMode().
    
    5     2001/09/29 3:36p Hirose
    Fixed emboss texgen bug.
    
    4     2001/09/24 3:19p Carl
    Fixed problem with cull mode conversion.
    
    3     2001/09/19 6:34p Carl
    Added GDSetLPSize.
    
    2     2001/09/14 6:42p Carl
    Fixed bug with GDSetVtxAttrFmtv.
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

/*---------------------------------------------------------------------------*/
//  Name:         GDSetVtxDescv
//
//  Description:  Sets vertex description registers.
//
//  Arguments:    attrPtr:   pointer to an (attr, type) array.
//                           End of the array is indicated by GX_VA_NULL 
//                           attribute.
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

void GDSetVtxDescv ( const GXVtxDescList *attrPtr )
{
    // These are default values for all the VCD fields.
    // You cannot change most of these defaults without adjusting
    // the code below as well.  The code assumes GX_NONE defaults
    // (for normals, colors, and texture coordinates).

    u32 nnorms = 0;
    u32 ncols  = 0;
    u32 ntexs  = 0;

    u32 pnMtxIdx = GX_NONE;
    u32 txMtxIdxMask = 0;
    u32 posn = GX_DIRECT;
    u32 norm = GX_NONE;
    u32 col0 = GX_NONE;
    u32 col1 = GX_NONE;
    u32 tex0 = GX_NONE;
    u32 tex1 = GX_NONE;
    u32 tex2 = GX_NONE;
    u32 tex3 = GX_NONE;
    u32 tex4 = GX_NONE;
    u32 tex5 = GX_NONE;
    u32 tex6 = GX_NONE;
    u32 tex7 = GX_NONE;

    while (attrPtr->attr != GX_VA_NULL) {
        ASSERTMSG(((attrPtr->attr >= GX_VA_PNMTXIDX) && 
                   (attrPtr->attr <= GX_VA_MAX_ATTR)),
                  "GDSetVtxDescv: invalid attribute");

        ASSERTMSG(((attrPtr->type >= GX_NONE) && 
                   (attrPtr->type <= GX_INDEX16)),
                  "GDSetVtxDescv: invalid type");

        ASSERTMSG(((attrPtr->attr >= GX_VA_PNMTXIDX) &&
                   (attrPtr->attr <= GX_VA_TEX7MTXIDX)) ?
                  ((attrPtr->type == GX_NONE) ||
                   (attrPtr->type == GX_DIRECT)) : 1,
                  "GDSetVtxDescv: invalid type for given attribute");

        switch (attrPtr->attr) {

          case GX_VA_PNMTXIDX: pnMtxIdx = attrPtr->type; break;

          case GX_VA_TEX0MTXIDX:
            txMtxIdxMask = (txMtxIdxMask &   ~1) | (attrPtr->type << 0); break;
          case GX_VA_TEX1MTXIDX:
            txMtxIdxMask = (txMtxIdxMask &   ~2) | (attrPtr->type << 1); break;
          case GX_VA_TEX2MTXIDX:
            txMtxIdxMask = (txMtxIdxMask &   ~4) | (attrPtr->type << 2); break;
          case GX_VA_TEX3MTXIDX:
            txMtxIdxMask = (txMtxIdxMask &   ~8) | (attrPtr->type << 3); break;
          case GX_VA_TEX4MTXIDX:
            txMtxIdxMask = (txMtxIdxMask &  ~16) | (attrPtr->type << 4); break;
          case GX_VA_TEX5MTXIDX:
            txMtxIdxMask = (txMtxIdxMask &  ~32) | (attrPtr->type << 5); break;
          case GX_VA_TEX6MTXIDX:
            txMtxIdxMask = (txMtxIdxMask &  ~64) | (attrPtr->type << 6); break;
          case GX_VA_TEX7MTXIDX:
            txMtxIdxMask = (txMtxIdxMask & ~128) | (attrPtr->type << 7); break;

          case GX_VA_POS: posn = attrPtr->type; break;

          case GX_VA_NRM:
            if (attrPtr->type != GX_NONE)
                { norm = attrPtr->type; nnorms = 1; }
            break;
          case GX_VA_NBT:
            if (attrPtr->type != GX_NONE)
                { norm = attrPtr->type; nnorms = 2; }
            break;

          case GX_VA_CLR0: col0=attrPtr->type; ncols+=(col0 != GX_NONE); break;
          case GX_VA_CLR1: col1=attrPtr->type; ncols+=(col1 != GX_NONE); break;

          case GX_VA_TEX0: tex0=attrPtr->type; ntexs+=(tex0 != GX_NONE); break;
          case GX_VA_TEX1: tex1=attrPtr->type; ntexs+=(tex1 != GX_NONE); break;
          case GX_VA_TEX2: tex2=attrPtr->type; ntexs+=(tex2 != GX_NONE); break;
          case GX_VA_TEX3: tex3=attrPtr->type; ntexs+=(tex3 != GX_NONE); break;
          case GX_VA_TEX4: tex4=attrPtr->type; ntexs+=(tex4 != GX_NONE); break;
          case GX_VA_TEX5: tex5=attrPtr->type; ntexs+=(tex5 != GX_NONE); break;
          case GX_VA_TEX6: tex6=attrPtr->type; ntexs+=(tex6 != GX_NONE); break;
          case GX_VA_TEX7: tex7=attrPtr->type; ntexs+=(tex7 != GX_NONE); break;
          default: break;
        }
        attrPtr++;
    }

    GDWriteCPCmd( CP_VCD_LO_ID, CP_VCD_REG_LO_PS( pnMtxIdx, txMtxIdxMask, 
                                                  posn, norm, col0, col1 ));

    GDWriteCPCmd( CP_VCD_HI_ID, CP_VCD_REG_HI( tex0, tex1, tex2, tex3,
                                               tex4, tex5, tex6, tex7 ));

    GDWriteXFCmd( XF_INVTXSPEC_ID, XF_INVTXSPEC( ncols, nnorms, ntexs ));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetVtxAttrFmt
//
//  Description:  Sets format of a given attribute in the vertex format array.
//
//  Arguments:    vtxFmt:	Name of the format array register.
//                list:      	pointer to an array of attribute format structures.
//
//  list structure elements:
//                attr:      	Name of the attribute.
//                cnt:       	Number of components for the attribute.
//                type:      	Data type of the attribute.
//                frac:      	number of fraction bits for int data types.
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

void GDSetVtxAttrFmtv ( GXVtxFmt vtxfmt, const GXVtxAttrFmtList *list )
{
    // These are default values for all the VAT fields.
    // You may change them as you wish.

    u32 posCnt  = GX_POS_XYZ;
    u32 posType = GX_F32;
    u32 posFrac = 0;

    u32 nrmCnt  = GX_NRM_XYZ;
    u32 nrmType = GX_F32;
    u32 nrmIdx3 = 0;
    
    u32 c0Cnt   = GX_CLR_RGBA;
    u32 c0Type  = GX_RGBA8;
    u32 c1Cnt   = GX_CLR_RGBA;
    u32 c1Type  = GX_RGBA8;
    
    u32 tx0Cnt  = GX_TEX_ST;
    u32 tx0Type = GX_F32;
    u32 tx0Frac = 0;
    u32 tx1Cnt  = GX_TEX_ST;
    u32 tx1Type = GX_F32;
    u32 tx1Frac = 0;
    u32 tx2Cnt  = GX_TEX_ST;
    u32 tx2Type = GX_F32;
    u32 tx2Frac = 0;
    u32 tx3Cnt  = GX_TEX_ST;
    u32 tx3Type = GX_F32;
    u32 tx3Frac = 0;
    u32 tx4Cnt  = GX_TEX_ST;
    u32 tx4Type = GX_F32;
    u32 tx4Frac = 0;
    u32 tx5Cnt  = GX_TEX_ST;
    u32 tx5Type = GX_F32;
    u32 tx5Frac = 0;
    u32 tx6Cnt  = GX_TEX_ST;
    u32 tx6Type = GX_F32;
    u32 tx6Frac = 0;
    u32 tx7Cnt  = GX_TEX_ST;
    u32 tx7Type = GX_F32;
    u32 tx7Frac = 0;
    
    ASSERTMSG((vtxfmt < GX_MAX_VTXFMT), "GDSetVtxAttrFmtv: invalid vtx fmt");

    while (list->attr != GX_VA_NULL) {
        ASSERTMSG((((list->attr >= GX_VA_POS) &&
                   (list->attr <= GX_VA_TEX7)) ||
                   (list->attr == GX_VA_NBT)),
                   "GDSetVtxAttrFmtv: invalid attribute");
        ASSERTMSG((list->frac < 32), "GDSetVtxAttrFmtv: invalid frac value");

        switch (list->attr) {
          case GX_VA_POS:
            posCnt  = list->cnt;
            posType = list->type;
            posFrac = list->frac;
            break;
          case GX_VA_NRM:
          case GX_VA_NBT:
            nrmType = list->type;
            if (list->cnt == GX_NRM_NBT3)
            {
                nrmCnt = GX_NRM_NBT;
                nrmIdx3 = 1;
            } else {
                nrmCnt = list->cnt;
                nrmIdx3 = 0;
            }
            break;
          case GX_VA_CLR0:
            c0Cnt  = list->cnt;
            c0Type = list->type;
            break;
          case GX_VA_CLR1:
            c1Cnt  = list->cnt;
            c1Type = list->type;
            break;
          case GX_VA_TEX0:
            tx0Cnt  = list->cnt;
            tx0Type = list->type;
            tx0Frac = list->frac;
            break;
          case GX_VA_TEX1:
            tx1Cnt  = list->cnt;
            tx1Type = list->type;
            tx1Frac = list->frac;
            break;
          case GX_VA_TEX2:
            tx2Cnt  = list->cnt;
            tx2Type = list->type;
            tx2Frac = list->frac;
            break;
          case GX_VA_TEX3:
            tx3Cnt  = list->cnt;
            tx3Type = list->type;
            tx3Frac = list->frac;
            break;
          case GX_VA_TEX4:
            tx4Cnt  = list->cnt;
            tx4Type = list->type;
            tx4Frac = list->frac;
            break;
          case GX_VA_TEX5:
            tx5Cnt  = list->cnt;
            tx5Type = list->type;
            tx5Frac = list->frac;
            break;
          case GX_VA_TEX6:
            tx6Cnt  = list->cnt;
            tx6Type = list->type;
            tx6Frac = list->frac;
            break;
          case GX_VA_TEX7:
            tx7Cnt  = list->cnt;
            tx7Type = list->type;
            tx7Frac = list->frac;
            break;
          default:
            break;
        }
        list++;
    }

    GDWriteCPCmd( (u8)(CP_VAT_A_ID + vtxfmt),
                  CP_VAT_REG_A(posCnt, posType, posFrac,
                               nrmCnt, nrmType,
                               c0Cnt,  c0Type, c1Cnt, c1Type,
                               tx0Cnt, tx0Type, tx0Frac,
                               nrmIdx3 ));
    
    GDWriteCPCmd( (u8)(CP_VAT_B_ID + vtxfmt),
                  CP_VAT_REG_B(tx1Cnt, tx1Type, tx1Frac,
                               tx2Cnt, tx2Type, tx2Frac,
                               tx3Cnt, tx3Type, tx3Frac,
                               tx4Cnt, tx4Type ));
    
    GDWriteCPCmd( (u8)(CP_VAT_C_ID + vtxfmt),
                  CP_VAT_REG_C(tx4Frac,
                               tx5Cnt, tx5Type, tx5Frac,
                               tx6Cnt, tx6Type, tx6Frac,
                               tx7Cnt, tx7Type, tx7Frac ));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetArray
//
//  Description:  Sets the base address and stride of data array for the given
//                attribute. This will be used by indexed vertex data.
//                The base address should be an EPPC cached memory address.
//
//  Arguments:    attr:        	Name of the attribute.
//                base_ptr:    	pointer to an array of data in cached memory.
//                stride:      	number of bytes between each attribute unit.
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

void GDSetArray ( GXAttr attr, const void *base_ptr, u8 stride )
{
    s32 cpAttr;

    cpAttr = (attr == GX_VA_NBT) ? (GX_VA_NRM - GX_VA_POS) : (attr - GX_VA_POS);
    
    GDWriteCPCmd( (u8) (CP_ARRAY_BASE_ID + cpAttr),
                  CP_ARRAY_BASE_REG( OSCachedToPhysical( base_ptr ) ));

    GDWriteCPCmd( (u8) (CP_ARRAY_STRIDE_ID + cpAttr),
                  CP_ARRAY_STRIDE_REG( stride ));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetArrayRaw
//
//  Description:  Sets the base address and stride of data array for the given
//                attribute. This will be used by indexed vertex data.
//                The base address is just a raw u32 number in this case.
//
//  Arguments:    attr:	          Name of the attribute.
//                base_ptr_raw:	  raw number to be inserted for base address.
//                stride:         number of bytes between each attribute unit.
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

void GDSetArrayRaw ( GXAttr attr, u32 base_ptr_raw, u8 stride )
{
    s32 cpAttr;

    cpAttr = (attr == GX_VA_NBT) ? (GX_VA_NRM - GX_VA_POS) : (attr - GX_VA_POS);
    
    GDWriteCPCmd( (u8) (CP_ARRAY_BASE_ID + cpAttr),
                  CP_ARRAY_BASE_REG( base_ptr_raw ));

    GDWriteCPCmd( (u8) (CP_ARRAY_STRIDE_ID + cpAttr),
                  CP_ARRAY_STRIDE_REG( stride ));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDPatchArrayPtr
//
//  Description:  Alters array base memory address for a GDSetArray command.
//                We assume that the DL offset has been moved to the point
//                CP_DATA_OFFSET bytes after where the GDSetArray command
//                was inserted.  Only the array base address is changed.
//
//  Arguments:    base_ptr:    		pointer to an array of data in cached memory.
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

void GDPatchArrayPtr ( const void *base_ptr )
{
    GDWrite_u32(CP_ARRAY_BASE_REG( OSCachedToPhysical( base_ptr ) ));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetTexCoordGen
//
//  Description:  Sets the texture coordinate control state in XF block.
//
//  Arguments:    dst_coord:   	Texture Coordinate Name.
//                func:        	Texture Generation Function.
//                src_param:   	Texture Generation Source coordinates.
//                mtx:         	Default texture matrix index.
//                normalize:   	Normalize results between transforms?
//                pt_texmtx:   	Post-transform matrix index.
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

void GDSetTexCoordGen ( GXTexCoordID  dst_coord,
                        GXTexGenType  func,
                        GXTexGenSrc   src_param,
                        GXBool        normalize,
                        u32           postmtx )
{
    u32     form;
    u32     tgType;
    u32     proj;
    u32     row;
    u32     embossRow;
    u32     embossLit;
    
    form      = XF_TEX_AB11;
    proj      = XF_TEX_ST;
    row       = XF_TEX0_INROW;
    embossRow = XF_TEX0_INROW;
    embossLit = 0;

    ASSERTMSG((dst_coord < GX_MAX_TEXCOORD),
              "GDSetTexCoordGen: invalid texcoord ID");

    switch (src_param) {
      case GX_TG_POS:     row = XF_GEOM_INROW;       form = XF_TEX_ABC1; break;
      case GX_TG_NRM:     row = XF_NORMAL_INROW;     form = XF_TEX_ABC1; break; 
      case GX_TG_BINRM:   row = XF_BINORMAL_T_INROW; form = XF_TEX_ABC1; break;
      case GX_TG_TANGENT: row = XF_BINORMAL_B_INROW; form = XF_TEX_ABC1; break;
      case GX_TG_COLOR0:  row = XF_COLORS_INROW; break;
      case GX_TG_COLOR1:  row = XF_COLORS_INROW; break;
      case GX_TG_TEX0:    row = XF_TEX0_INROW;   break;
      case GX_TG_TEX1:    row = XF_TEX1_INROW;   break;
      case GX_TG_TEX2:    row = XF_TEX2_INROW;   break;
      case GX_TG_TEX3:    row = XF_TEX3_INROW;   break;
      case GX_TG_TEX4:    row = XF_TEX4_INROW;   break;
      case GX_TG_TEX5:    row = XF_TEX5_INROW;   break;
      case GX_TG_TEX6:    row = XF_TEX6_INROW;   break;
      case GX_TG_TEX7:    row = XF_TEX7_INROW;   break;
      case GX_TG_TEXCOORD0: embossRow = 0; break;
      case GX_TG_TEXCOORD1: embossRow = 1; break;
      case GX_TG_TEXCOORD2: embossRow = 2; break;
      case GX_TG_TEXCOORD3: embossRow = 3; break;
      case GX_TG_TEXCOORD4: embossRow = 4; break;
      case GX_TG_TEXCOORD5: embossRow = 5; break;
      case GX_TG_TEXCOORD6: embossRow = 6; break;
      default:
        ASSERTMSG(0, "GDSetTexCoordGen: invalid texgen source");
        break;
    }

    switch (func) {
      case GX_TG_MTX2x4:
        tgType = XF_TEXGEN_REGULAR;
        break;
        
      case GX_TG_MTX3x4:
        tgType = XF_TEXGEN_REGULAR;
        proj   = XF_TEX_STQ;
        break;

      case GX_TG_BUMP0:
      case GX_TG_BUMP1:
      case GX_TG_BUMP2:
      case GX_TG_BUMP3:
      case GX_TG_BUMP4:
      case GX_TG_BUMP5:
      case GX_TG_BUMP6:
      case GX_TG_BUMP7:
        ASSERTMSG((src_param >= GX_TG_TEXCOORD0) && 
                  (src_param <= GX_TG_TEXCOORD6),
                  "GDSetTexCoordGen: invalid emboss source");
        tgType = XF_TEXGEN_EMBOSS_MAP;
        embossLit = (u32) (func - GX_TG_BUMP0);
        break;

      case GX_TG_SRTG:
        if (src_param == GX_TG_COLOR0) {
            tgType = XF_TEXGEN_COLOR_STRGBC0;
        } else {
            tgType = XF_TEXGEN_COLOR_STRGBC1;
        }
        break;

      default:
        ASSERTMSG(0, "GDSetTexCoordGen: invalid texgen function");
        break;
    }

    GDWriteXFCmd( (u16) (XF_TEX0_ID + dst_coord),
                  XF_TEX( proj, form, tgType, row,
                          embossRow, embossLit ));

    //---------------------------------------------------------------------
    // Update DUALTEX state
    //---------------------------------------------------------------------

    GDWriteXFCmd( (u16) (XF_DUALTEX0_ID + dst_coord), XF_DUALTEX(
        (postmtx - GX_PTTEXMTX0), normalize ));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetCullMode
//
//  Description:  Sets front/back face culling mode. The register mask
//                is used to changing anything other than cull mode.
//                Note: The HW's notion of frontface is opposite of the 
//                api's notion of front face so we need to translate the
//                enums.
//
//  Arguments:    mode:          cull mode.
//                
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

#define GEN_MODE_MASK_SETCULLMODE \
    ( 0x000003 << GEN_MODE_REJECT_EN_SHIFT )

void GDSetCullMode( GXCullMode mode )
{
    static u8 cm2hw[] = { 0, 2, 1, 3 };

    GDWriteBPCmd( SS_MASK( GEN_MODE_MASK_SETCULLMODE ));

    GDWriteBPCmd( GEN_MODE( 0, 0, 0, 0, cm2hw[mode], 0, 0, GEN_MODE_ID ));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetGenMode
//
//  Description:  Sets the gen mode register for the raster pipe.
//                The register mask is used to prevent changing the cull mode,
//                the coplanar bit, AA mode, or number of indirect stages.
//
//  Arguments:    nTexGens:    Number of texgens.
//                nChans:      Number of active lighting channels.
//                nTevs:       Number of active TEV stages.
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

#define GEN_MODE_MASK_SETGENMODE \
    (( 0x00000F << GEN_MODE_NTEX_SHIFT ) | \
     ( 0x000003 << GEN_MODE_NCOL_SHIFT ) | \
     ( 0x00000F << GEN_MODE_NTEV_SHIFT ))


void GDSetGenMode( u8 nTexGens, u8 nChans, u8 nTevs )
{
    GDWriteBPCmd( SS_MASK( GEN_MODE_MASK_SETGENMODE ));

    GDWriteBPCmd( GEN_MODE( nTexGens, nChans, 0, (nTevs-1),
                            0, 0, 0, GEN_MODE_ID ));

    GDWriteXFCmd( XF_NUMCOLORS_ID, XF_NUMCOLORS( nChans ));
    GDWriteXFCmd( XF_NUMTEX_ID, XF_NUMTEX( nTexGens ));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetGenMode2
//
//  Description:  Sets the gen mode register for the raster pipe.
//                Also lets you specify the culling mode.  The mask is used
//                to prevent changing the coplanar bit or AA mode.
//
//  NOTE:         The indirect mask is not dealt with!  If you change the
//                number of indirect stages, you'd better also set the
//                indirect mask as appropriate.
//
//  Arguments:    nTexGens:    Number of texgens.
//                nChans:      Number of active lighting channels.
//                nTev:        Number of active TEV stages.
//                nInds:       Number of active indirect texture stages.
//                cm:          Front/back-face culling mode.
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

#define GEN_MODE_MASK_SETGENMODE2 \
    (( 0x00000F << GEN_MODE_NTEX_SHIFT )      | \
     ( 0x000003 << GEN_MODE_NCOL_SHIFT )      | \
     ( 0x00000F << GEN_MODE_NTEV_SHIFT )      | \
     ( 0x000003 << GEN_MODE_REJECT_EN_SHIFT ) | \
     ( 0x000007 << GEN_MODE_NBMP_SHIFT ))


void GDSetGenMode2( u8 nTexGens, u8 nChans, u8 nTevs, u8 nInds, GXCullMode cm )
{
    static u8 cm2hw[] = { 0, 2, 1, 3 };

    GDWriteBPCmd( SS_MASK( GEN_MODE_MASK_SETGENMODE2 ));

    GDWriteBPCmd( GEN_MODE( nTexGens, nChans, 0, (nTevs-1),
                            cm2hw[cm], nInds, 0, GEN_MODE_ID ));

    GDWriteXFCmd( XF_NUMCOLORS_ID, XF_NUMCOLORS( nChans ));
    GDWriteXFCmd( XF_NUMTEX_ID, XF_NUMTEX( nTexGens ));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetLPSize
//
//  Description:  Sets the line width & texture offset, point size &
//                texture offset, and line aspect ratio.
//
//  Arguments:    lineWidth:        	line width, in 1/6's
//                pointSize:        	point size, in 1/6's
//                lineOffset:       	line texture offset
//                pointOffset:      	point texture offset
//                lineHalfAspect:   	TRUE for drawing lines in field mode
//                                      FALSE for drawing lines in frame mode
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

void GDSetLPSize( u8 lineWidth, u8 pointSize,
                  GXTexOffset lineOffset, GXTexOffset pointOffset,
                  GXBool lineHalfAspect )
{
    GDWriteBPCmd( SU_LPSIZE( lineWidth, pointSize,
                             lineOffset, pointOffset,
                             lineHalfAspect, SU_LPSIZE_ID ));
}

/*---------------------------------------------------------------------------*/
//  Name:         GDSetCoPlanar
//
//  Description:  Sets the coplanar mode within the gen mode register.
//                This freezes the Z slopes of the last triangle sent.
//                All subsequent triangles will use the same Z slopes,
//                meaning that they will be coplanar.  The register mask
//                is used to set only this bit within gen mode.
//
//  Arguments:    enable:      GX_TRUE to enable coplanar mode
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

#define GEN_MODE_MASK_SETCOPLANAR \
    ( 0x000001 << GEN_MODE_ZFREEZE_SHIFT )

void GDSetCoPlanar( GXBool enable )
{
    GDWriteBPCmd( SS_MASK( GEN_MODE_MASK_SETCOPLANAR ));

    GDWriteBPCmd( GEN_MODE( 0, 0, 0, 0, 0, 0, enable, GEN_MODE_ID ));
}

