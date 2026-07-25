/*---------------------------------------------------------------------------*
  Project:  Dolphin GD library
  File:     GDTransform.c

  $Log: GDTransform.c,v $
  Revision 1.2  2006/02/20 04:24:39  mitu
  Changed include path from dolphin/ to revolution/.

  Revision 1.1.1.1  2005/05/12 02:15:49  yasuh-to
  Ported from dolphin source tree.

    
    4     2002/08/05 19:53 Hirose
    Const type specifier support.
    
    3     2002/05/30 13:40 Hirose
    Added GDSetTransform.
    
    2     2001/09/24 3:25p Carl
    Fixed off-by-1 count in GDLoadTexMtxImm.
    
    1     2001/09/12 1:52p Carl
    Initial revision of GD: Graphics Display List Library.

  $NoKeywords: $
 *---------------------------------------------------------------------------*/

#include <revolution/gd.h>
#include <revolution/os.h>

/*---------------------------------------------------------------------------*/
//  Name:        GDLoadPosMtxImm 
//
//  Description: Loads position viewing matrix into xfrom block. 
//
//  Arguments:    mtx:     viewing matrix
//                id:      index of the matrix register file.
//
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

void GDLoadPosMtxImm ( const f32 mtx[3][4], u32 id )
{
    GDWriteXFCmdHdr(__PosMtxToXFMem(id), (u8)(3*4));
    
    GDWrite_f32(mtx[0][0]);
    GDWrite_f32(mtx[0][1]);
    GDWrite_f32(mtx[0][2]);
    GDWrite_f32(mtx[0][3]);
               
    GDWrite_f32(mtx[1][0]);
    GDWrite_f32(mtx[1][1]);
    GDWrite_f32(mtx[1][2]);
    GDWrite_f32(mtx[1][3]);
               
    GDWrite_f32(mtx[2][0]);
    GDWrite_f32(mtx[2][1]);
    GDWrite_f32(mtx[2][2]);
    GDWrite_f32(mtx[2][3]);
}

/*---------------------------------------------------------------------------*/
//  Name:         GDLoadPosMtxIndx
//
//  Description:  Loads viewing matrix directly from memory.
//
//  Arguments:    mtx_indx:        Index into matrix array in main memory.
//                id:              Index of the matrix register file.
//
//  Returns:      
//
/*---------------------------------------------------------------------------*/

void GDLoadPosMtxIndx(u16 mtx_indx, u32 id)
{
    GDWriteXFIndxACmd(__PosMtxToXFMem(id), (u8)(3*4), mtx_indx);
}

/*---------------------------------------------------------------------------*/
//  Name:         GDLoadNrmMtxImm
//
//  Description:  Loads normal matrix in to xform block
//
//  Arguments:    mtx:     normal matrix data (3x4 matrix).
//                id:      Index of the normal matrix register file.
//
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

void GDLoadNrmMtxImm ( const f32 mtx[3][4], u32 id )
{
    GDWriteXFCmdHdr(__NrmMtxToXFMem(id), (u8)(3*3));

    GDWrite_f32(mtx[0][0]);
    GDWrite_f32(mtx[0][1]);
    GDWrite_f32(mtx[0][2]);
               
    GDWrite_f32(mtx[1][0]);
    GDWrite_f32(mtx[1][1]);
    GDWrite_f32(mtx[1][2]);
               
    GDWrite_f32(mtx[2][0]);
    GDWrite_f32(mtx[2][1]);
    GDWrite_f32(mtx[2][2]);
}

/*---------------------------------------------------------------------------*/
//  Name:         GDLoadNrmMtxImm3x3
//
//  Description:  Loads normal matrix in to xform block
//
//  Arguments:    mtx:     normal matrix data (3x3 matrix).
//                id:      Index of the normal matrix register file.
//
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

void GDLoadNrmMtxImm3x3 ( const f32 mtx[3][3], u32 id )
{
    GDWriteXFCmdHdr(__NrmMtxToXFMem(id), (u8)(3*3));

    GDWrite_f32(mtx[0][0]);
    GDWrite_f32(mtx[0][1]);
    GDWrite_f32(mtx[0][2]);
               
    GDWrite_f32(mtx[1][0]);
    GDWrite_f32(mtx[1][1]);
    GDWrite_f32(mtx[1][2]);
               
    GDWrite_f32(mtx[2][0]);
    GDWrite_f32(mtx[2][1]);
    GDWrite_f32(mtx[2][2]);
}

/*---------------------------------------------------------------------------*/
//  Name:         GDLoadNrmMtxIndx3x3
//
//  Description:  Load normal matrix directly from external memory.
//
//  Arguments:    mtx_indx:        Index into matrix array in main memory
//                              (3x3 matrices).
//                id:              Index of the matrix register file.
//
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

void GDLoadNrmMtxIndx3x3 ( u16 mtx_indx, u32 id )
{
    GDWriteXFIndxBCmd(__NrmMtxToXFMem(id), (u8)(3*3), mtx_indx);
}

/*---------------------------------------------------------------------------*/
//  Name:         GDLoadTexMtxImm
//
//  Description:  Loads texture matrix.
//
//  Arguments:    mtx:     matrix data.
//                id:      index of matrix in xf block's matrix array.
//                type:    type of matrix (2x4 or 3x4)
//
//  Returns:      none.
//
/*---------------------------------------------------------------------------*/

void GDLoadTexMtxImm ( const f32 mtx[3][4], u32 id, GXTexMtxType type )
{
    u16     addr;
    u8      count;

    if (id >= GX_PTTEXMTX0) 
    {
        ASSERTMSG(type == GX_MTX3x4, "GDLoadTexMtxImm: invalid matrix type");
        addr  = __DTTMtxToXFMem(id);
        count = (u8)(3*4);
    } else {
        addr  = __TexMtxToXFMem(id);
        count = (u8)((type == GX_MTX2x4) ? (2*4) : (3*4));
    }

    GDWriteXFCmdHdr(addr, count);

    GDWrite_f32(mtx[0][0]);
    GDWrite_f32(mtx[0][1]);
    GDWrite_f32(mtx[0][2]);
    GDWrite_f32(mtx[0][3]);
               
    GDWrite_f32(mtx[1][0]);
    GDWrite_f32(mtx[1][1]);
    GDWrite_f32(mtx[1][2]);
    GDWrite_f32(mtx[1][3]);
               
    if (type == GX_MTX3x4) 
    {
        GDWrite_f32(mtx[2][0]);
        GDWrite_f32(mtx[2][1]);
        GDWrite_f32(mtx[2][2]);
        GDWrite_f32(mtx[2][3]);
    }
}

/*---------------------------------------------------------------------------*/
//  Name:         GDLoadTexMtxIndx
//
//  Description:  Load texture matrix directly from memory.
//
//  Arguments:    mtx_indx:        Index into matrix array in main memory.
//                id:              Index of the matrix register file.
//                type:            type of matrix (2x4 or 3x4)
//
//  Returns:      None.
//
/*---------------------------------------------------------------------------*/

void GDLoadTexMtxIndx ( u16 mtx_indx, u32 id, GXTexMtxType type )
{
    u16     addr;
    u8      count;

    if (id >= GX_PTTEXMTX0) 
    {
        ASSERTMSG(type == GX_MTX3x4, "GDLoadTexMtxIndx: invalid matrix type");
        addr  = __DTTMtxToXFMem(id);
        count = (u8)(3*4);
    } else {
        addr  = __TexMtxToXFMem(id);
        count = (u8)((type == GX_MTX2x4) ? (2*4) : (3*4));
    }

    GDWriteXFIndxCCmd(addr, count, mtx_indx);
}




/*---------------------------------------------------------------------------*/
//  Name:         GDSetCurrentMtx  
//
//  Description:  Sets the index of default position & normal matrix.
//
//  Arguments:    pn...t7:         index in the matrix register array in xf block.
//                
//  Returns:      none
//
/*---------------------------------------------------------------------------*/

void GDSetCurrentMtx ( u32 pn, u32 t0, u32 t1, u32 t2, u32 t3,
                       u32 t4, u32 t5, u32 t6, u32 t7 )
{
    u32 regA;
    u32 regB;

    regA = MATIDX_REG_A(pn, t0, t1, t2, t3);
    regB = MATIDX_REG_B(t4, t5, t6, t7);

    GDWriteCPCmd( CP_MATINDEX_A, regA ); // W1
    GDWriteCPCmd( CP_MATINDEX_B, regB ); // W2

    GDWriteXFCmdHdr( XF_MATINDEX_A, 2 );
    GDWrite_u32( regA ); // W1
    GDWrite_u32( regB ); // W2
}


/*---------------------------------------------------------------------------*/
//  Name:         GDSetProjection
//
//  Description:  Sets projection matrix parameters.
//
//  Arguments:    mtx:     Projection Matrix.
//                type:    Projection matrix type (ORTHO or PERSP)
//                
//  The six values in XF memory (i.e.XF_PROJECTIONA..XF_PROJECTIONF)
//  corresponds to matrix data as follows:
//
//  Orthographic Matrix:    [A 0 0 B]
//                          [0 C 0 D]
//                          [0 0 E F]
//                          [0 0 0 1]
//
//  Perspective Matrix:     [A 0 B 0]
//                          [0 C D 0]
//                          [0 0 E F]
//                          [0 0 -1 0]
//
//  Returns:      None
//
/*---------------------------------------------------------------------------*/

void GDSetProjection ( const f32 mtx[4][4], GXProjectionType type )
{
    u32 c;

    c = ( type == GX_ORTHOGRAPHIC ) ? 3u : 2u;

    GDWriteXFCmdHdr( XF_PROJECTION_ID, 7 );
    GDWrite_f32( mtx[0][0] );
    GDWrite_f32( mtx[0][c] );
    GDWrite_f32( mtx[1][1] );
    GDWrite_f32( mtx[1][c] );
    GDWrite_f32( mtx[2][2] );
    GDWrite_f32( mtx[2][3] );
    GDWrite_u32( type );
}


