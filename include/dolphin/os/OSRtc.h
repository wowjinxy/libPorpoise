/*---------------------------------------------------------------------------*
  OSRtc.h - Real-Time Clock and SRAM
 *---------------------------------------------------------------------------*/

#ifndef DOLPHIN_OSRTC_H
#define DOLPHIN_OSRTC_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SRAM structure (64 bytes of persistent configuration) */
typedef struct OSSram {
    u16 checkSum;
    u16 checkSumInv;
    u32 counterBias;
    s8  displayOffsetH;
    u8  ntd;
    u8  language;
    u8  flags;
} OSSram;

typedef struct OSSramEx {
    u8  flashID[2][12];
    u32 wirelessKeyboardID;
    u16 wirelessPadID[4];
    u8  dvdErrorCode;
    u8  pad1;
    u8  flashIDCheckSum[2];
    u16 gbs;
    u8  pad2[2];
} OSSramEx;

/* SRAM flag bits */
#define OS_SRAM_VIDEO_MODE          0x03
#define OS_SRAM_SOUND_MODE          0x04
#define OS_SRAM_PROGRESSIVE_FLAG    0x80

#define OS_SRAM_VIDEO_MODE_SHIFT    0
#define OS_SRAM_SOUND_MODE_SHIFT    2
#define OS_SRAM_PROGRESSIVE_SHIFT   7

/* Video modes */
#define OS_VIDEO_MODE_NTSC  0
#define OS_VIDEO_MODE_PAL   1
#define OS_VIDEO_MODE_MPAL  2

/* Sound modes */
#define OS_SOUND_MODE_MONO      0
#define OS_SOUND_MODE_STEREO    1

/* Progressive modes */
#define OS_PROGRESSIVE_MODE_OFF 0
#define OS_PROGRESSIVE_MODE_ON  1

/* Languages */
#define OS_LANG_ENGLISH     0
#define OS_LANG_GERMAN      1
#define OS_LANG_FRENCH      2
#define OS_LANG_SPANISH     3
#define OS_LANG_ITALIAN     4
#define OS_LANG_DUTCH       5
#define OS_LANG_JAPANESE    6

/* RTC Functions */
BOOL __OSGetRTC(u32* rtc);
BOOL __OSSetRTC(u32 rtc);

/* SRAM Functions */
OSSram*   __OSLockSram(void);
OSSramEx* __OSLockSramEx(void);
BOOL      __OSUnlockSram(BOOL commit);
BOOL      __OSUnlockSramEx(BOOL commit);
BOOL      __OSSyncSram(void);

/* Settings */
u32  OSGetSoundMode(void);
void OSSetSoundMode(u32 mode);
u32  OSGetProgressiveMode(void);
void OSSetProgressiveMode(u32 on);
u32  OSGetVideoMode(void);
void OSSetVideoMode(u32 mode);
u8   OSGetLanguage(void);
void OSSetLanguage(u8 language);

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OSRTC_H */

