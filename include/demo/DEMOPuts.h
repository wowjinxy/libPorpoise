#ifndef __DEMOPUTS_H__
#define __DEMOPUTS_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DMTF_POINTSAMPLE,
    DMTF_BILERP
} DMTexFlt;

#define DM_FT_OPQ   0
#define DM_FT_RVS   1
#define DM_FT_XLU   2

extern void DEMOSetFontType(s32);
extern void DEMOSetupScrnSpc(s32, s32, f32);
extern void DEMOInitCaption(s32, s32, s32);
extern void DEMOLoadFont(GXTexMapID, GXTexMtx, DMTexFlt);

extern void DEMOPuts(s16, s16, s16, char*);
extern void DEMOPrintf(s16, s16, s16, char*, ...);

extern OSFontHeader* DEMOInitROMFont(void);
extern void DEMOSetROMFontSize(s16 size, s16 space);
extern void DEMOGetROMFontSize(s16* size, s16* space);
extern int DEMOGetRFTextWidth(char* string);
extern int DEMOGetRFTextHeight(char* string);

extern int DEMORFPuts(s16 x, s16 y, s16 z, char* string);
extern int DEMORFPutsEx(s16 x, s16 y, s16 z, char* string, s16 maxWidth, int length);
extern int DEMORFPrintf(s16 x, s16 y, s16 z, char* fmt, ...);

extern char* DEMODumpROMFont(char* string);

extern u32 DEMOFontBitmap[];



#ifdef __cplusplus
}
#endif

#endif

