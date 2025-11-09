/*---------------------------------------------------------------------------*
  OSFont.c - Font and Character Encoding
  
  HARDWARE FONT SYSTEM (GC/Wii):
  ==============================
  
  The original GameCube/Wii had built-in fonts stored in the IPL (Initial
  Program Loader) ROM:
  
  1. **ANSI Font** (~77 KB):
     - ASCII characters (0x20-0x7E)
     - Extended Latin characters (0xA0-0xFF)
     - 12x12 fixed-width bitmap font
     
  2. **Shift-JIS Font** (~2.6 MB):
     - Japanese characters (Hiragana, Katakana, Kanji)
     - Variable-width bitmap font
     - Thousands of characters
  
  **Font Data Access:**
  - Fonts loaded from IPL ROM at address 0x81800000 (physical)
  - OSLoadFont() loads into memory
  - OSInitFont() initializes the font system
  - OSGetFontTexture() renders characters to GX textures
  
  **Character Encoding:**
  - ANSI (1 byte per character)
  - Shift-JIS (1-2 bytes per character)
  - UTF-8 (1-4 bytes per character)
  - UTF-16 (2 or 4 bytes per character)
  - UTF-32 (4 bytes per character)
  
  PC PORT STRATEGY:
  =================
  
  1. **Font Rendering (NOT IMPLEMENTED):**
     - We don't have IPL ROM fonts
     - Games should use their own font rendering:
       * TrueType fonts (FreeType, stb_truetype)
       * Bitmap fonts (custom format)
       * System fonts (platform-specific)
     - OSInitFont, OSLoadFont, OSGetFontTexture → Stubs
  
  2. **UTF Conversion (IMPLEMENTED):**
     - These are useful general-purpose utilities
     - Work independently of font rendering
     - Fully implemented and tested
  
  MIGRATION FOR GAMES:
  ====================
  
  Games using OSFont for rendering should:
  1. Replace with custom font system (FreeType recommended)
  2. Keep UTF conversion functions (they work!)
  3. Port font data to TrueType or bitmap format
  
  Most games don't use OSFont - they use custom text rendering.
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <string.h>

/* Global state (stubs) */
static u16 s_fontEncode = OS_FONT_ENCODE_ANSI;
static BOOL s_fixedWidth = OS_FONT_PROPORTIONAL;

/*---------------------------------------------------------------------------*
  Name:         OSGetFontEncode

  Description:  Gets the current font encoding mode.
                
                On original hardware: Returns encoding set by OSSetFontEncode
                or auto-detected from system region.
                
                On PC: Returns ANSI by default (stub).

  Returns:      Font encoding (OS_FONT_ENCODE_ANSI, etc.)
 *---------------------------------------------------------------------------*/
u16 OSGetFontEncode(void) {
    /* Original hardware checks:
     * - VI region (NTSC/PAL/MPAL)
     * - System menu language setting
     * - Returns ANSI for US/EU, SJIS for Japan
     * 
     * PC: We just return ANSI.
     */
    return s_fontEncode;
}

/*---------------------------------------------------------------------------*
  Name:         OSSetFontWidth

  Description:  Sets fixed vs proportional width rendering.

  Arguments:    fixed - TRUE for fixed width, FALSE for proportional

  Returns:      Previous fixed width setting
 *---------------------------------------------------------------------------*/
BOOL OSSetFontWidth(BOOL fixed) {
    BOOL prev = s_fixedWidth;
    s_fixedWidth = fixed;
    return prev;
}

/*===========================================================================*
  FONT RENDERING STUBS
  
  These functions require the IPL ROM fonts which don't exist on PC.
  Games using these should port to TrueType/FreeType or similar.
 *===========================================================================*/

/*---------------------------------------------------------------------------*
  Name:         OSInitFont

  Description:  Initializes the font system with the specified font data.
                
                On original hardware: Sets up font rendering, decompresses
                font data, initializes character lookup tables.
                
                On PC: STUB - Returns FALSE (not implemented).

  Arguments:    fontData - Pointer to OSFontHeader structure

  Returns:      TRUE on success, FALSE on failure
 *---------------------------------------------------------------------------*/
BOOL OSInitFont(OSFontHeader* fontData) {
    /* Original hardware:
     * - Decompresses font texture sheets
     * - Builds character code lookup tables
     * - Sets up GX texture format (I4 = 4-bit intensity)
     * 
     * PC: Font data doesn't exist. Games should use their own fonts.
     */
    (void)fontData;
    return FALSE;  /* Not implemented - use custom font system */
}

/*---------------------------------------------------------------------------*
  Name:         OSLoadFont

  Description:  Loads font data from IPL ROM.
                
                On original hardware: Reads font from ROM address, expands
                compressed data into provided buffer.
                
                On PC: STUB - Returns 0 (no font data).

  Arguments:    fontData - Destination buffer
                temp     - Temporary buffer for decompression

  Returns:      Size of loaded font data (0 = failure)
 *---------------------------------------------------------------------------*/
u32 OSLoadFont(OSFontHeader* fontData, void* temp) {
    /* Original hardware:
     * - Reads from IPL ROM at physical address 0x81800000
     * - ANSI font offset: IPL_FONT_OFFSET
     * - SJIS font offset: IPL_FONT_OFFSET + 0x12D00
     * - Uses LZ compression, decompresses with temp buffer
     * 
     * PC: No IPL ROM. Return 0.
     */
    (void)fontData;
    (void)temp;
    return 0;  /* No font data available */
}

/*---------------------------------------------------------------------------*
  Name:         OSGetFontWidth

  Description:  Calculates the pixel width of a string.
                
                On original hardware: Parses string, sums character widths
                from font width table.
                
                On PC: STUB - Returns 0 width.

  Arguments:    string - String to measure
                width  - Pointer to receive width (can be NULL)

  Returns:      Pointer to end of string (after last character)
 *---------------------------------------------------------------------------*/
char* OSGetFontWidth(const char* string, s32* width) {
    /* Original hardware:
     * - Parses each character (respecting encoding)
     * - Looks up width in fontData->widthTable
     * - Sums total width in pixels
     * 
     * PC: No font data, return 0 width.
     */
    if (width) {
        *width = 0;
    }
    
    /* Return end of string */
    if (string) {
        return (char*)(string + strlen(string));
    }
    return (char*)string;
}

/*---------------------------------------------------------------------------*
  Name:         OSGetFontTexture

  Description:  Renders a character to a GX texture and returns metrics.
                
                On original hardware: Looks up character in font sheet,
                copies texel data to provided image buffer.
                
                On PC: STUB - Returns NULL image.

  Arguments:    string - String to render (first character)
                image  - Pointer to receive texture pointer
                x      - Pointer to receive x position in sheet
                y      - Pointer to receive y position in sheet
                width  - Pointer to receive character width

  Returns:      Pointer to next character in string
 *---------------------------------------------------------------------------*/
char* OSGetFontTexture(const char* string, void** image, s32* x, s32* y, s32* width) {
    /* Original hardware:
     * - ParseString to get font and character code
     * - Calculate sheet position: col = code % sheetColumn, row = code / sheetColumn
     * - Return pointer to character in font sheet texture
     * 
     * PC: No font sheets.
     */
    if (image) *image = NULL;
    if (x) *x = 0;
    if (y) *y = 0;
    if (width) *width = 0;
    
    /* Skip one character and return */
    if (string && *string) {
        return (char*)(string + 1);
    }
    return (char*)string;
}

/*---------------------------------------------------------------------------*
  Name:         OSGetFontTexel

  Description:  Copies character texel data to a destination texture.
                
                On original hardware: Blits character bitmap from font sheet
                to dest texture at specified position.
                
                On PC: STUB - Does nothing.

  Arguments:    string - String to render (first character)
                image  - Destination texture
                pos    - Horizontal position in texture
                stride - Width of texture in bytes / 2
                width  - Pointer to receive character width

  Returns:      Pointer to next character in string
 *---------------------------------------------------------------------------*/
char* OSGetFontTexel(const char* string, void* image, s32 pos, s32 stride, s32* width) {
    /* Original hardware:
     * - Gets character texel data from font sheet
     * - Copies to image at position 'pos' with given stride
     * - Used for building text textures
     * 
     * PC: No font data.
     */
    (void)image;
    (void)pos;
    (void)stride;
    
    if (width) *width = 0;
    
    /* Skip one character */
    if (string && *string) {
        return (char*)(string + 1);
    }
    return (char*)string;
}

/*===========================================================================*
  UTF CONVERSION FUNCTIONS
  
  These are FULLY IMPLEMENTED as they're general-purpose utilities
  independent of the font rendering system.
 *===========================================================================*/

/*---------------------------------------------------------------------------*
  Name:         OSUTF8to32

  Description:  Converts UTF-8 encoded string to UTF-32 code point.

  Arguments:    utf8  - UTF-8 string (1-4 bytes)
                utf32 - Pointer to receive UTF-32 code point

  Returns:      Pointer to next character in UTF-8 string
  
  UTF-8 Encoding:
    0xxxxxxx                    → 0x00000000 - 0x0000007F (ASCII)
    110xxxxx 10xxxxxx           → 0x00000080 - 0x000007FF
    1110xxxx 10xxxxxx 10xxxxxx  → 0x00000800 - 0x0000FFFF
    11110xxx 10xxxxxx 10xxxxxx 10xxxxxx → 0x00010000 - 0x0010FFFF
 *---------------------------------------------------------------------------*/
char* OSUTF8to32(const char* utf8, u32* utf32) {
    if (!utf8 || !utf32) {
        if (utf32) *utf32 = 0;
        return (char*)utf8;
    }
    
    const u8* str = (const u8*)utf8;
    u32 result = 0;
    u8 byte1 = str[0];
    
    if (byte1 == 0) {
        /* Null terminator */
        *utf32 = 0;
        return (char*)utf8;
    }
    else if ((byte1 & 0x80) == 0) {
        /* 1-byte sequence: 0xxxxxxx */
        result = byte1;
        utf8 += 1;
    }
    else if ((byte1 & 0xE0) == 0xC0) {
        /* 2-byte sequence: 110xxxxx 10xxxxxx */
        result = (byte1 & 0x1F) << 6;
        result |= (str[1] & 0x3F);
        utf8 += 2;
    }
    else if ((byte1 & 0xF0) == 0xE0) {
        /* 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx */
        result = (byte1 & 0x0F) << 12;
        result |= (str[1] & 0x3F) << 6;
        result |= (str[2] & 0x3F);
        utf8 += 3;
    }
    else if ((byte1 & 0xF8) == 0xF0) {
        /* 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
        result = (byte1 & 0x07) << 18;
        result |= (str[1] & 0x3F) << 12;
        result |= (str[2] & 0x3F) << 6;
        result |= (str[3] & 0x3F);
        utf8 += 4;
    }
    else {
        /* Invalid UTF-8 sequence */
        result = 0xFFFD;  /* Unicode replacement character */
        utf8 += 1;
    }
    
    *utf32 = result;
    return (char*)utf8;
}

/*---------------------------------------------------------------------------*
  Name:         OSUTF32to8

  Description:  Converts UTF-32 code point to UTF-8 string.

  Arguments:    utf32 - UTF-32 code point
                utf8  - Buffer for UTF-8 output (must be at least 5 bytes)

  Returns:      Pointer to byte after last written byte (for chaining)
 *---------------------------------------------------------------------------*/
char* OSUTF32to8(u32 utf32, char* utf8) {
    if (!utf8) return NULL;
    
    u8* out = (u8*)utf8;
    
    if (utf32 <= 0x7F) {
        /* 1-byte sequence */
        out[0] = (u8)utf32;
        return (char*)(out + 1);
    }
    else if (utf32 <= 0x7FF) {
        /* 2-byte sequence */
        out[0] = 0xC0 | (u8)(utf32 >> 6);
        out[1] = 0x80 | (u8)(utf32 & 0x3F);
        return (char*)(out + 2);
    }
    else if (utf32 <= 0xFFFF) {
        /* 3-byte sequence */
        out[0] = 0xE0 | (u8)(utf32 >> 12);
        out[1] = 0x80 | (u8)((utf32 >> 6) & 0x3F);
        out[2] = 0x80 | (u8)(utf32 & 0x3F);
        return (char*)(out + 3);
    }
    else if (utf32 <= 0x10FFFF) {
        /* 4-byte sequence */
        out[0] = 0xF0 | (u8)(utf32 >> 18);
        out[1] = 0x80 | (u8)((utf32 >> 12) & 0x3F);
        out[2] = 0x80 | (u8)((utf32 >> 6) & 0x3F);
        out[3] = 0x80 | (u8)(utf32 & 0x3F);
        return (char*)(out + 4);
    }
    else {
        /* Invalid code point - output replacement character */
        out[0] = 0xEF;  /* U+FFFD in UTF-8 */
        out[1] = 0xBF;
        out[2] = 0xBD;
        return (char*)(out + 3);
    }
}

/*---------------------------------------------------------------------------*
  Name:         OSUTF16to32

  Description:  Converts UTF-16 to UTF-32 code point.
                Handles surrogate pairs for code points > U+FFFF.

  Arguments:    utf16 - UTF-16 string (1 or 2 u16 values)
                utf32 - Pointer to receive UTF-32 code point

  Returns:      Pointer to next u16 in UTF-16 string
  
  UTF-16 Encoding:
    0x0000 - 0xD7FF: Single u16 (same as UTF-32)
    0xD800 - 0xDBFF: High surrogate (first of pair)
    0xDC00 - 0xDFFF: Low surrogate (second of pair)
    0xE000 - 0xFFFF: Single u16 (same as UTF-32)
 *---------------------------------------------------------------------------*/
u16* OSUTF16to32(const u16* utf16, u32* utf32) {
    if (!utf16 || !utf32) {
        if (utf32) *utf32 = 0;
        return (u16*)utf16;
    }
    
    u16 high = utf16[0];
    
    if (high == 0) {
        /* Null terminator */
        *utf32 = 0;
        return (u16*)utf16;
    }
    else if (high >= 0xD800 && high <= 0xDBFF) {
        /* High surrogate - need to read low surrogate */
        u16 low = utf16[1];
        if (low >= 0xDC00 && low <= 0xDFFF) {
            /* Valid surrogate pair */
            u32 result = 0x10000;
            result += ((high & 0x3FF) << 10);
            result += (low & 0x3FF);
            *utf32 = result;
            return (u16*)(utf16 + 2);
        }
        else {
            /* Invalid - missing low surrogate */
            *utf32 = 0xFFFD;  /* Replacement character */
            return (u16*)(utf16 + 1);
        }
    }
    else if (high >= 0xDC00 && high <= 0xDFFF) {
        /* Unexpected low surrogate */
        *utf32 = 0xFFFD;
        return (u16*)(utf16 + 1);
    }
    else {
        /* Regular BMP character */
        *utf32 = high;
        return (u16*)(utf16 + 1);
    }
}

/*---------------------------------------------------------------------------*
  Name:         OSUTF32to16

  Description:  Converts UTF-32 code point to UTF-16.

  Arguments:    utf32 - UTF-32 code point
                utf16 - Buffer for UTF-16 output (must be at least 2 u16)

  Returns:      Pointer to u16 after last written value
 *---------------------------------------------------------------------------*/
u16* OSUTF32to16(u32 utf32, u16* utf16) {
    if (!utf16) return NULL;
    
    if (utf32 <= 0xFFFF) {
        /* BMP character - single u16 */
        if (utf32 >= 0xD800 && utf32 <= 0xDFFF) {
            /* Surrogate range is invalid for direct encoding */
            utf16[0] = 0xFFFD;  /* Replacement character */
        } else {
            utf16[0] = (u16)utf32;
        }
        return utf16 + 1;
    }
    else if (utf32 <= 0x10FFFF) {
        /* Supplementary plane - surrogate pair */
        u32 adjusted = utf32 - 0x10000;
        utf16[0] = 0xD800 | (u16)((adjusted >> 10) & 0x3FF);
        utf16[1] = 0xDC00 | (u16)(adjusted & 0x3FF);
        return utf16 + 2;
    }
    else {
        /* Invalid code point */
        utf16[0] = 0xFFFD;
        return utf16 + 1;
    }
}

/*---------------------------------------------------------------------------*
  Name:         OSUTF32toANSI

  Description:  Converts UTF-32 to ANSI (8-bit Latin-1).
                Code points > 0xFF are replaced with '?'.

  Arguments:    utf32 - UTF-32 code point

  Returns:      ANSI character (0x00-0xFF)
 *---------------------------------------------------------------------------*/
u8 OSUTF32toANSI(u32 utf32) {
    /* ANSI/Latin-1 is just the first 256 Unicode code points */
    if (utf32 <= 0xFF) {
        return (u8)utf32;
    }
    else {
        return (u8)'?';  /* Replacement for unmappable characters */
    }
}

/*---------------------------------------------------------------------------*
  Name:         OSANSItoUTF32

  Description:  Converts ANSI (8-bit Latin-1) to UTF-32.

  Arguments:    ansi - ANSI character

  Returns:      UTF-32 code point
 *---------------------------------------------------------------------------*/
u32 OSANSItoUTF32(u8 ansi) {
    /* ANSI/Latin-1 maps directly to first 256 Unicode code points */
    return (u32)ansi;
}

/*---------------------------------------------------------------------------*
  Name:         OSUTF32toSJIS / OSSJIStoUTF32

  Description:  Shift-JIS conversion (STUB - requires large conversion tables).
                
                Shift-JIS is a Japanese character encoding used on GC/Wii.
                Proper conversion requires large lookup tables (~thousands
                of entries).

  Arguments:    utf32 / sjis - Code points to convert

  Returns:      Converted value (or 0 if unmappable)
  
  Note:         For real SJIS conversion, use a library like ICU or iconv.
 *---------------------------------------------------------------------------*/
u16 OSUTF32toSJIS(u32 utf32) {
    /* Shift-JIS conversion requires large Unicode→SJIS tables.
     * The original SDK has these tables, but they're huge.
     * 
     * For ASCII range, SJIS is identical to ASCII.
     */
    if (utf32 <= 0x7F) {
        return (u16)utf32;
    }
    
    /* For real conversion, use ICU, iconv, or include SJIS tables */
    return 0;  /* Unmappable */
}

u32 OSSJIStoUTF32(u16 sjis) {
    /* Shift-JIS → Unicode conversion requires large tables.
     * 
     * For ASCII range (single byte), SJIS is identical to ASCII.
     */
    if (sjis <= 0x7F) {
        return (u32)sjis;
    }
    
    /* For real conversion, use ICU, iconv, or include SJIS tables */
    return 0xFFFD;  /* Replacement character */
}
