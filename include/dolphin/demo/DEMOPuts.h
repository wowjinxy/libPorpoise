/*---------------------------------------------------------------------------*
  Project:  libPorpoise Demo Library
  File:     DEMOPuts.h
  
  Text rendering for demo framework.
  
  Based on Nintendo's Revolution SDK demo library.
 *---------------------------------------------------------------------------*/

#ifndef DOLPHIN_DEMO_PUTS_H
#define DOLPHIN_DEMO_PUTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dolphin/types.h>
#include <dolphin/gx/GXEnum.h>
#include <dolphin/os/OSFont.h>

/*---------------------------------------------------------------------------*
    DEMOPuts.c
 *---------------------------------------------------------------------------*/
// filtering type for DEMOLoadFont texture
typedef enum
{
    DMTF_POINTSAMPLE,   // Point sampling
    DMTF_BILERP         // Bilerp filtering
} DMTexFlt;

// caption font type
#define DM_FT_OPQ   0
#define DM_FT_RVS   1
#define DM_FT_XLU   2

// DEMOFont functions
extern  void            DEMOSetFontType ( s32 );
extern  void            DEMOSetupScrnSpc( s32, s32, f32 );
extern  void            DEMOInitCaption ( s32, s32, s32 );
extern  void            DEMOLoadFont    ( GXTexMapID, GXTexMtx, DMTexFlt );

extern  void            DEMOPuts        ( s16, s16, s16, char* );
extern  void            DEMOPrintf      ( s16, s16, s16, char*, ... );

// ROM font functions
extern  OSFontHeader*   DEMOInitROMFont     ( void );
extern  void            DEMOSetROMFontSize  ( s16 size, s16 space );
extern  void            DEMOGetROMFontSize  ( s16* size, s16* space );
extern  int             DEMOGetRFTextWidth  ( char* string );
extern  int             DEMOGetRFTextHeight ( char* string );

extern  int             DEMORFPuts          ( s16 x, s16 y, s16 z, char* string );
extern  int             DEMORFPutsEx        ( s16 x, s16 y, s16 z, char* string, s16 maxWidth, int length);
extern  int             DEMORFPrintf        ( s16 x, s16 y, s16 z, char* fmt, ... );

extern  char*           DEMODumpROMFont     ( char* string );

/*---------------------------------------------------------------------------*
    DEMOFont.c
 *---------------------------------------------------------------------------*/
extern  u32 DEMOFontBitmap[];

/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_DEMO_PUTS_H */

