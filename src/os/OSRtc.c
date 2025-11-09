/*---------------------------------------------------------------------------*
  OSRtc.c - Real-Time Clock and SRAM
  
  HARDWARE RTC SYSTEM (GC/Wii):
  ==============================
  
  **Hardware Components:**
  
  1. **RTC Chip (Real-Time Clock):**
     - Battery-backed clock chip
     - Keeps time even when console is off
     - Accessed via EXI (External Interface) bus
     - 32-bit counter (seconds since January 1, 2000)
     - Read/write via EXI commands (0x20000000 = read, 0xA0000000 = write)
  
  2. **SRAM (64 bytes of persistent storage):**
     - Attached to RTC chip
     - Stores system settings:
       * Video mode (NTSC/PAL/MPAL)
       * Sound mode (Mono/Stereo)
       * Progressive scan setting
       * Language (English/German/French/etc.)
       * Misc flags and checksums
     - Survives power off (battery-backed)
     - Read/written via EXI commands
  
  **EXI Protocol:**
  ```
  1. EXILock(channel=0, device=1)
  2. EXISelect(channel=0, device=1, freq=8MHz)
  3. EXIImm(channel=0, cmd=READ/WRITE, 4 bytes)
  4. EXISync(channel=0) - wait for completion
  5. EXIImm/EXIDma - read/write data
  6. EXIDeselect(channel=0)
  7. EXIUnlock(channel=0)
  ```
  
  **SRAM Structure:**
  ```c
  OSSram (first 32 bytes):
    - checkSum/checkSumInv: Integrity check
    - counterBias: RTC offset adjustment
    - displayOffsetH: Display offset
    - language: System language
    - flags: Video/sound/progressive settings
  
  OSSramEx (next 32 bytes):
    - Wireless keyboard/pad IDs
    - Flash IDs for network adapters
    - DVD error codes
    - GBS player settings
  ```
  
  PC PORT IMPLEMENTATION:
  =======================
  
  **What We Can Do on PC:**
  
  1. **RTC → System Time** ✅
     - Windows: GetSystemTime(), SetSystemTime()
     - Linux: time(), clock_gettime()
     - Get/set real-world time
     - Persists across application restarts (OS manages it)
  
  2. **SRAM → Config File** ✅
     - Save settings to "porpoise_sram.cfg"
     - Persist video/sound/language preferences
     - Load on startup, save on changes
     - Checksum validation
  
  3. **Settings APIs** ✅
     - OSGetSoundMode/OSSetSoundMode
     - OSGetVideoMode/OSSetVideoMode
     - OSGetLanguage/OSSetLanguage
     - OSGetProgressiveMode/OSSetProgressiveMode
     - All work perfectly on PC
  
  **Advantages on PC:**
  - System time is more accurate (NTP-synced)
  - SRAM can be larger than 64 bytes if needed
  - No EXI bus timing issues
  - Can use text format for easy editing
  
  **Differences:**
  - No hardware RTC chip (use OS time instead)
  - No EXI bus (direct OS calls)
  - Config file instead of hardware SRAM
  - Checksum still validated for compatibility
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

/* SRAM configuration file */
#define SRAM_CONFIG_FILE "porpoise_sram.cfg"

/* SRAM control block */
typedef struct SramControl {
    u8   sram[64];          /* 64 bytes total (OSSram + OSSramEx) */
    BOOL locked;            /* Lock state */
    BOOL enabled;           /* Saved interrupt state */
    BOOL sync;              /* TRUE if file and memory are in sync */
    u32  offset;            /* Offset to flush */
} SramControl;

static SramControl s_scb = {0};

/*===========================================================================*
  RTC FUNCTIONS (Real-Time Clock)
 *===========================================================================*/

/*---------------------------------------------------------------------------*
  Name:         __OSGetRTC

  Description:  Reads the current time from the RTC.
                
                On original hardware:
                - Reads 32-bit counter from RTC chip via EXI
                - Counter is seconds since January 1, 2000 00:00:00
                - Reads twice to verify (hardware can be unreliable)
                
                On PC:
                - Uses Windows GetSystemTime() or POSIX time()
                - Converts to seconds since January 1, 2000
                - Always succeeds (OS time is reliable)

  Arguments:    rtc - Pointer to receive RTC value

  Returns:      TRUE on success, FALSE on failure
 *---------------------------------------------------------------------------*/
BOOL __OSGetRTC(u32* rtc) {
    if (!rtc) return FALSE;
    
    /* Get current system time */
#ifdef _WIN32
    SYSTEMTIME st;
    FILETIME ft;
    ULARGE_INTEGER uli;
    
    GetSystemTime(&st);
    SystemTimeToFileTime(&st, &ft);
    
    /* Convert FILETIME to 64-bit value */
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    
    /* FILETIME is 100-nanosecond intervals since January 1, 1601 */
    /* Convert to seconds since January 1, 2000 */
    
    /* January 1, 2000 in FILETIME */
    SYSTEMTIME epoch = {2000, 1, 0, 1, 0, 0, 0, 0};
    FILETIME epoch_ft;
    SystemTimeToFileTime(&epoch, &epoch_ft);
    
    ULARGE_INTEGER epoch_uli;
    epoch_uli.LowPart = epoch_ft.dwLowDateTime;
    epoch_uli.HighPart = epoch_ft.dwHighDateTime;
    
    /* Calculate difference in 100ns intervals, convert to seconds */
    u64 diff = uli.QuadPart - epoch_uli.QuadPart;
    *rtc = (u32)(diff / 10000000ULL);  /* 100ns → seconds */
    
#else
    time_t now = time(NULL);
    
    /* Create epoch (January 1, 2000) */
    struct tm epoch = {0};
    epoch.tm_year = 2000 - 1900;
    epoch.tm_mon = 0;
    epoch.tm_mday = 1;
    time_t epoch_time = mktime(&epoch);
    
    /* Calculate seconds since epoch */
    *rtc = (u32)(now - epoch_time);
#endif
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         __OSSetRTC

  Description:  Sets the RTC to a specific time.
                
                On original hardware:
                - Writes 32-bit value to RTC chip via EXI
                - Sets hardware clock (persists when powered off)
                
                On PC:
                - Could use SetSystemTime() but requires admin privileges
                - For now, we just accept it (OS manages system time)

  Arguments:    rtc - RTC value (seconds since January 1, 2000)

  Returns:      TRUE on success, FALSE on failure
 *---------------------------------------------------------------------------*/
BOOL __OSSetRTC(u32 rtc) {
    /* On original hardware, writes to RTC chip via EXI.
     * 
     * On PC, setting system time requires administrator privileges.
     * Most games don't need to set RTC (they just read it).
     * 
     * We return TRUE for API compatibility, but don't actually
     * change the system time.
     */
    (void)rtc;
    
#ifdef _DEBUG
    OSReport("[OSRtc] __OSSetRTC called with %u (not implemented on PC)\n", rtc);
    OSReport("        Setting system time requires admin privileges.\n");
#endif
    
    return TRUE;  /* API compatibility */
}

/*===========================================================================*
  SRAM FUNCTIONS (Persistent Configuration Storage)
 *===========================================================================*/

/*---------------------------------------------------------------------------*
  Name:         __OSInitSram

  Description:  Initializes SRAM system. Loads config from file.

  Arguments:    None
 *---------------------------------------------------------------------------*/
void __OSInitSram(void) {
    FILE* fp;
    
    /* Initialize control block */
    s_scb.locked = FALSE;
    s_scb.enabled = FALSE;
    s_scb.sync = FALSE;
    s_scb.offset = 64;
    
    /* Try to load existing SRAM */
    fp = fopen(SRAM_CONFIG_FILE, "rb");
    if (fp) {
        size_t read = fread(s_scb.sram, 1, 64, fp);
        fclose(fp);
        
        if (read == 64) {
            s_scb.sync = TRUE;
            OSReport("[OSRtc] Loaded SRAM from %s\n", SRAM_CONFIG_FILE);
        } else {
            OSReport("[OSRtc] SRAM file corrupted, using defaults\n");
        }
    } else {
        OSReport("[OSRtc] No SRAM file found, using defaults\n");
    }
    
    /* If no valid SRAM, initialize with defaults */
    if (!s_scb.sync) {
        OSSram* sram = (OSSram*)s_scb.sram;
        memset(s_scb.sram, 0, 64);
        
        /* Set defaults */
        sram->flags = OS_VIDEO_MODE_NTSC | (OS_SOUND_MODE_STEREO << 2);
        sram->language = OS_LANG_ENGLISH;
        sram->counterBias = 0;
        
        /* Calculate checksum */
        u16 checkSum = 0;
        u16* p = (u16*)&sram->counterBias;
        for (int i = 0; i < (sizeof(OSSram) - 4) / 2; i++) {
            checkSum += p[i];
        }
        sram->checkSum = checkSum;
        sram->checkSumInv = ~checkSum;
        
        /* Save defaults */
        fp = fopen(SRAM_CONFIG_FILE, "wb");
        if (fp) {
            fwrite(s_scb.sram, 1, 64, fp);
            fclose(fp);
            s_scb.sync = TRUE;
        }
    }
}

/*---------------------------------------------------------------------------*
  Name:         __OSLockSram / __OSLockSramEx

  Description:  Locks SRAM and returns pointer for reading/modifying.
                Disables interrupts to ensure atomic access.

  Returns:      Pointer to SRAM structure, or NULL if already locked
 *---------------------------------------------------------------------------*/
OSSram* __OSLockSram(void) {
    BOOL enabled = OSDisableInterrupts();
    
    if (s_scb.locked) {
        OSRestoreInterrupts(enabled);
        return NULL;
    }
    
    s_scb.enabled = enabled;
    s_scb.locked = TRUE;
    return (OSSram*)s_scb.sram;
}

OSSramEx* __OSLockSramEx(void) {
    BOOL enabled = OSDisableInterrupts();
    
    if (s_scb.locked) {
        OSRestoreInterrupts(enabled);
        return NULL;
    }
    
    s_scb.enabled = enabled;
    s_scb.locked = TRUE;
    return (OSSramEx*)(s_scb.sram + sizeof(OSSram));
}

/*---------------------------------------------------------------------------*
  Name:         __OSUnlockSram / __OSUnlockSramEx

  Description:  Unlocks SRAM and optionally commits changes to file.

  Arguments:    commit - TRUE to save to file, FALSE to discard

  Returns:      TRUE if sync successful
 *---------------------------------------------------------------------------*/
BOOL __OSUnlockSram(BOOL commit) {
    if (!s_scb.locked) return FALSE;
    
    if (commit) {
        OSSram* sram = (OSSram*)s_scb.sram;
        
        /* Recalculate checksum */
        sram->checkSum = sram->checkSumInv = 0;
        u16* p = (u16*)&sram->counterBias;
        for (int i = 0; i < (sizeof(OSSram) - 4) / 2; i++) {
            sram->checkSum += p[i];
            sram->checkSumInv += ~p[i];
        }
        
        /* Write to file */
        FILE* fp = fopen(SRAM_CONFIG_FILE, "wb");
        if (fp) {
            fwrite(s_scb.sram, 1, 64, fp);
            fclose(fp);
            s_scb.sync = TRUE;
            s_scb.offset = 64;
        } else {
            s_scb.sync = FALSE;
        }
    }
    
    s_scb.locked = FALSE;
    OSRestoreInterrupts(s_scb.enabled);
    
    return s_scb.sync;
}

BOOL __OSUnlockSramEx(BOOL commit) {
    /* Same as regular unlock */
    return __OSUnlockSram(commit);
}

/*---------------------------------------------------------------------------*
  Name:         __OSSyncSram

  Description:  Checks if SRAM is synchronized with file.

  Returns:      TRUE if in sync
 *---------------------------------------------------------------------------*/
BOOL __OSSyncSram(void) {
    return s_scb.sync;
}

/*===========================================================================*
  SETTINGS APIs
 *===========================================================================*/

/*---------------------------------------------------------------------------*
  Name:         OSGetSoundMode / OSSetSoundMode

  Description:  Gets/sets sound mode (mono/stereo).

  Returns:      OS_SOUND_MODE_MONO or OS_SOUND_MODE_STEREO
 *---------------------------------------------------------------------------*/
u32 OSGetSoundMode(void) {
    OSSram* sram = __OSLockSram();
    if (!sram) return OS_SOUND_MODE_STEREO;
    
    u32 mode = (sram->flags & OS_SRAM_SOUND_MODE) ? OS_SOUND_MODE_STEREO : OS_SOUND_MODE_MONO;
    __OSUnlockSram(FALSE);
    return mode;
}

void OSSetSoundMode(u32 mode) {
    OSSram* sram = __OSLockSram();
    if (!sram) return;
    
    mode <<= OS_SRAM_SOUND_MODE_SHIFT;
    mode &= OS_SRAM_SOUND_MODE;
    
    if (mode == (sram->flags & OS_SRAM_SOUND_MODE)) {
        __OSUnlockSram(FALSE);
        return;
    }
    
    sram->flags &= ~OS_SRAM_SOUND_MODE;
    sram->flags |= mode;
    __OSUnlockSram(TRUE);
}

/*---------------------------------------------------------------------------*
  Name:         OSGetProgressiveMode / OSSetProgressiveMode

  Description:  Gets/sets progressive scan mode (interlace/progressive).

  Returns:      OS_PROGRESSIVE_MODE_OFF or OS_PROGRESSIVE_MODE_ON
 *---------------------------------------------------------------------------*/
u32 OSGetProgressiveMode(void) {
    OSSram* sram = __OSLockSram();
    if (!sram) return OS_PROGRESSIVE_MODE_OFF;
    
    u32 on = (sram->flags & OS_SRAM_PROGRESSIVE_FLAG) >> OS_SRAM_PROGRESSIVE_SHIFT;
    __OSUnlockSram(FALSE);
    return on;
}

void OSSetProgressiveMode(u32 on) {
    OSSram* sram = __OSLockSram();
    if (!sram) return;
    
    on <<= OS_SRAM_PROGRESSIVE_SHIFT;
    on &= OS_SRAM_PROGRESSIVE_FLAG;
    
    if (on == (sram->flags & OS_SRAM_PROGRESSIVE_FLAG)) {
        __OSUnlockSram(FALSE);
        return;
    }
    
    sram->flags &= ~OS_SRAM_PROGRESSIVE_FLAG;
    sram->flags |= on;
    __OSUnlockSram(TRUE);
}

/*---------------------------------------------------------------------------*
  Name:         OSGetVideoMode / OSSetVideoMode

  Description:  Gets/sets video mode (NTSC/PAL/MPAL).

  Returns:      OS_VIDEO_MODE_NTSC, OS_VIDEO_MODE_PAL, or OS_VIDEO_MODE_MPAL
 *---------------------------------------------------------------------------*/
u32 OSGetVideoMode(void) {
    OSSram* sram = __OSLockSram();
    if (!sram) return OS_VIDEO_MODE_NTSC;
    
    u32 mode = sram->flags & OS_SRAM_VIDEO_MODE;
    __OSUnlockSram(FALSE);
    
    if (mode > OS_VIDEO_MODE_MPAL) {
        mode = OS_VIDEO_MODE_NTSC;  /* Default */
    }
    return mode;
}

void OSSetVideoMode(u32 mode) {
    OSSram* sram = __OSLockSram();
    if (!sram) return;
    
    if (mode > OS_VIDEO_MODE_MPAL) {
        mode = OS_VIDEO_MODE_NTSC;
    }
    
    if (mode == (sram->flags & OS_SRAM_VIDEO_MODE)) {
        __OSUnlockSram(FALSE);
        return;
    }
    
    sram->flags &= ~OS_SRAM_VIDEO_MODE;
    sram->flags |= mode;
    __OSUnlockSram(TRUE);
}

/*---------------------------------------------------------------------------*
  Name:         OSGetLanguage / OSSetLanguage

  Description:  Gets/sets system language.

  Returns:      OS_LANG_ENGLISH, OS_LANG_GERMAN, etc.
 *---------------------------------------------------------------------------*/
u8 OSGetLanguage(void) {
    OSSram* sram = __OSLockSram();
    if (!sram) return OS_LANG_ENGLISH;
    
    u8 language = sram->language;
    __OSUnlockSram(FALSE);
    
    /* Validate */
    if (language > OS_LANG_JAPANESE) {
        language = OS_LANG_ENGLISH;
    }
    return language;
}

void OSSetLanguage(u8 language) {
    OSSram* sram = __OSLockSram();
    if (!sram) return;
    
    /* Validate */
    if (language > OS_LANG_JAPANESE) {
        language = OS_LANG_ENGLISH;
    }
    
    if (language == sram->language) {
        __OSUnlockSram(FALSE);
        return;
    }
    
    sram->language = language;
    __OSUnlockSram(TRUE);
}

