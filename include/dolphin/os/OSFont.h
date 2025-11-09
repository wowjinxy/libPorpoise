#ifndef DOLPHIN_OSFONT_H
#define DOLPHIN_OSFONT_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSFontHeader
{
    u16 fontType;
    u16 firstChar;
    u16 lastChar;
    u16 invalChar;
    u16 ascent;
    u16 descent;
    u16 width;
    u16 leading;
    u16 cellWidth;
    u16 cellHeight;
    u32 sheetSize;
    u16 sheetFormat;
    u16 sheetColumn;
    u16 sheetRow;
    u16 sheetWidth;
    u16 sheetHeight;
    u16 widthTable;
    u32 sheetImage;
    u32 sheetFullSize;
    u8  c0;
    u8  c1;
    u8  c2;
    u8  c3;
} OSFontHeader;

#define OS_FONT_ENCODE_ANSI     0u
#define OS_FONT_ENCODE_SJIS     1u
#define OS_FONT_ENCODE_UTF8     3u
#define OS_FONT_ENCODE_UTF16    4u
#define OS_FONT_ENCODE_UTF32    5u
#define OS_FONT_ENCODE_MAX      5u
#define OS_FONT_ENCODE_VOID     0xffffu

#define OS_FONT_PROPORTIONAL    FALSE
#define OS_FONT_FIXED           TRUE

u16   OSGetFontEncode  (void);
char* OSGetFontWidth   (const char* string, s32* width);
BOOL  OSInitFont       (OSFontHeader* fontData);
char* OSGetFontTexture (const char* string, void** image, s32* x, s32* y, s32* width);
u32   OSLoadFont       (OSFontHeader* fontData, void* temp);
char* OSGetFontTexel   (const char* string, void* image, s32 pos, s32 stride, s32* width);
char* OSUTF32to8       (u32 utf32, char* utf8);
char* OSUTF8to32       (const char* utf8, u32* utf32);
u16*  OSUTF32to16      (u32 utf32, u16* utf16);
u16*  OSUTF16to32      (const u16* utf16, u32* utf32);
u8    OSUTF32toANSI    (u32 utf32);
u32   OSANSItoUTF32    (u8 ansi);
u16   OSUTF32toSJIS    (u32 utf32);
u32   OSSJIStoUTF32    (u16 sjis);
BOOL  OSSetFontWidth   (BOOL fixed);

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OSFONT_H */

