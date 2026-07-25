#ifndef DOLPHIN_GXDRAW_H
#define DOLPHIN_GXDRAW_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void GXDrawSphere(u8 numMajor, u8 numMinor);
void GXDrawSphere1(u8 depth);
void GXDrawCylinder(u8 numEdges);
void GXDrawTorus(f32 rc, u8 numc, u8 numt);
void GXDrawCube(void);
void GXDrawDodeca(void);
void GXDrawOctahedron(void);
void GXDrawIcosahedron(void);
u32 GXGenNormalTable(u8 depth, f32* table);

#ifdef __cplusplus
}
#endif

#endif
