#include <dolphin/os.h>

u16 OSGetFontEncode(void) {
    return OS_FONT_ENCODE_ANSI;
}

char* OSGetFontWidth(const char* string, s32* width) {
    if (width) *width = 0;
    return (char*)string;
}

BOOL OSInitFont(OSFontHeader* fontData) {
    return FALSE;
}

char* OSGetFontTexture(const char* string, void** image, s32* x, s32* y, s32* width) {
    return (char*)string;
}

u32 OSLoadFont(OSFontHeader* fontData, void* temp) {
    return 0;
}

char* OSGetFontTexel(const char* string, void* image, s32 pos, s32 stride, s32* width) {
    return (char*)string;
}

char* OSUTF32to8(u32 utf32, char* utf8) {
    if (utf8) *utf8 = (char)utf32;
    return utf8;
}

char* OSUTF8to32(const char* utf8, u32* utf32) {
    if (utf32 && utf8) *utf32 = (u32)*utf8;
    return (char*)utf8;
}

u16* OSUTF32to16(u32 utf32, u16* utf16) {
    if (utf16) *utf16 = (u16)utf32;
    return utf16;
}

u16* OSUTF16to32(const u16* utf16, u32* utf32) {
    if (utf32 && utf16) *utf32 = (u32)*utf16;
    return (u16*)utf16;
}

u8 OSUTF32toANSI(u32 utf32) {
    return (u8)utf32;
}

u32 OSANSItoUTF32(u8 ansi) {
    return (u32)ansi;
}

u16 OSUTF32toSJIS(u32 utf32) {
    return (u16)utf32;
}

u32 OSSJIStoUTF32(u16 sjis) {
    return (u32)sjis;
}

BOOL OSSetFontWidth(BOOL fixed) {
    return fixed;
}

