/**
 * @file vi.h
 * @brief Video Interface (VI) API for libPorpoise
 * 
 * Manages frame buffers and display output.
 * On PC, this provides stubs - actual rendering handled by game's graphics system.
 */

#ifndef DOLPHIN_VI_H
#define DOLPHIN_VI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dolphin/types.h>

/*---------------------------------------------------------------------------*
    Constants
 *---------------------------------------------------------------------------*/

// Display pixel size
#define VI_DISPLAY_PIX_SZ           2

// Interlace modes
#define VI_INTERLACE                0
#define VI_NON_INTERLACE            1
#define VI_PROGRESSIVE              2

// TV formats
#define VI_NTSC                     0
#define VI_PAL                      1
#define VI_MPAL                     2
#define VI_DEBUG                    3
#define VI_DEBUG_PAL                4
#define VI_EURGB60                  5

// TV mode macro
#define VI_TVMODE(FMT, INT)   (((FMT) << 2) + (INT))

// Field types
#define VI_FIELD_ABOVE              1
#define VI_FIELD_BELOW              0

// Maximum screen dimensions
#define VI_MAX_WIDTH_NTSC           720
#define VI_MAX_HEIGHT_NTSC          480

#define VI_MAX_WIDTH_PAL            720
#define VI_MAX_HEIGHT_PAL           574

#define VI_MAX_WIDTH_MPAL           720
#define VI_MAX_HEIGHT_MPAL          480

#define VI_MAX_WIDTH_EURGB60        VI_MAX_WIDTH_NTSC
#define VI_MAX_HEIGHT_EURGB60       VI_MAX_HEIGHT_NTSC

// XFB (External Frame Buffer) modes
#define VI_XFBMODE_SF               0  // Single field
#define VI_XFBMODE_DF               1  // Double field

/*---------------------------------------------------------------------------*
    Types
 *---------------------------------------------------------------------------*/

/**
 * @brief TV mode enumeration
 */
typedef enum {
    VI_TVMODE_NTSC_INT      = VI_TVMODE(VI_NTSC,        VI_INTERLACE),
    VI_TVMODE_NTSC_DS       = VI_TVMODE(VI_NTSC,        VI_NON_INTERLACE),
    VI_TVMODE_NTSC_PROG     = VI_TVMODE(VI_NTSC,        VI_PROGRESSIVE),
    
    VI_TVMODE_PAL_INT       = VI_TVMODE(VI_PAL,         VI_INTERLACE),
    VI_TVMODE_PAL_DS        = VI_TVMODE(VI_PAL,         VI_NON_INTERLACE),
    
    VI_TVMODE_EURGB60_INT   = VI_TVMODE(VI_EURGB60,     VI_INTERLACE),
    VI_TVMODE_EURGB60_DS    = VI_TVMODE(VI_EURGB60,     VI_NON_INTERLACE),
    VI_TVMODE_EURGB60_PROG  = VI_TVMODE(VI_EURGB60,     VI_PROGRESSIVE),
    
    VI_TVMODE_MPAL_INT      = VI_TVMODE(VI_MPAL,        VI_INTERLACE),
    VI_TVMODE_MPAL_DS       = VI_TVMODE(VI_MPAL,        VI_NON_INTERLACE),
    VI_TVMODE_MPAL_PROG     = VI_TVMODE(VI_MPAL,        VI_PROGRESSIVE),
    
    VI_TVMODE_DEBUG_INT     = VI_TVMODE(VI_DEBUG,       VI_INTERLACE),
    VI_TVMODE_DEBUG_PAL_INT = VI_TVMODE(VI_DEBUG_PAL,   VI_INTERLACE),
    VI_TVMODE_DEBUG_PAL_DS  = VI_TVMODE(VI_DEBUG_PAL,   VI_NON_INTERLACE)
} VITVMode;

/**
 * @brief Retrace callback function type
 */
typedef void (*VIRetraceCallback)(u32 retraceCount);

/**
 * @brief Position callback function type
 */
typedef void (*VIPositionCallback)(s16 x, s16 y);

/**
 * @brief Render mode object (from GX, referenced by VI)
 */
typedef struct GXRenderModeObj GXRenderModeObj;

/*---------------------------------------------------------------------------*
    Functions
 *---------------------------------------------------------------------------*/

/**
 * @brief Initialize video interface
 * 
 * On GC/Wii: Initializes VI hardware registers
 * On PC: Stub - game handles window/graphics
 */
void VIInit(void);

/**
 * @brief Wait for vertical retrace (VBlank)
 * 
 * Blocks until next vertical blanking interval.
 * On PC: Sleeps for ~16ms (60Hz) or ~20ms (50Hz)
 */
void VIWaitForRetrace(void);

/**
 * @brief Flush VI configuration to hardware
 * 
 * On GC/Wii: Writes shadow registers to VI hardware
 * On PC: No-op stub
 */
void VIFlush(void);

/**
 * @brief Set next frame buffer
 * 
 * @param fb  Pointer to frame buffer
 */
void VISetNextFrameBuffer(void* fb);

/**
 * @brief Get next frame buffer
 * 
 * @return Pointer to next frame buffer
 */
void* VIGetNextFrameBuffer(void);

/**
 * @brief Get current frame buffer
 * 
 * @return Pointer to current frame buffer
 */
void* VIGetCurrentFrameBuffer(void);

/**
 * @brief Set next right frame buffer (for 3D)
 * 
 * @param fb  Pointer to right eye frame buffer
 */
void VISetNextRightFrameBuffer(void* fb);

/**
 * @brief Enable/disable black screen
 * 
 * @param black  TRUE to show black, FALSE to show frame buffer
 */
void VISetBlack(BOOL black);

/**
 * @brief Enable/disable 3D mode
 * 
 * @param threeD  TRUE for 3D, FALSE for 2D
 */
void VISet3D(BOOL threeD);

/**
 * @brief Configure video mode
 * 
 * @param rm  Pointer to render mode object
 */
void VIConfigure(const GXRenderModeObj* rm);

/**
 * @brief Configure panning
 * 
 * @param xOrg    X origin
 * @param yOrg    Y origin
 * @param width   Width
 * @param height  Height
 */
void VIConfigurePan(u16 xOrg, u16 yOrg, u16 width, u16 height);

/**
 * @brief Get retrace count
 * 
 * @return Number of vertical retraces since boot
 */
u32 VIGetRetraceCount(void);

/**
 * @brief Get next field type
 * 
 * @return VI_FIELD_ABOVE or VI_FIELD_BELOW
 */
u32 VIGetNextField(void);

/**
 * @brief Get current scan line
 * 
 * @return Current line being drawn
 */
u32 VIGetCurrentLine(void);

/**
 * @brief Get TV format
 * 
 * @return VI_NTSC, VI_PAL, VI_MPAL, or VI_EURGB60
 */
u32 VIGetTvFormat(void);

/**
 * @brief Get scan mode (interlace/progressive)
 * 
 * @return VI_INTERLACE, VI_NON_INTERLACE, or VI_PROGRESSIVE
 */
u32 VIGetScanMode(void);

/**
 * @brief Get DTV status
 * 
 * @return DTV status flags
 */
u32 VIGetDTVStatus(void);

/**
 * @brief Set retrace callback
 * 
 * @param callback  Callback function called on each VBlank
 * @return Previous callback
 */
VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback callback);

/**
 * @brief Set post-retrace callback
 * 
 * @param callback  Callback function
 * @return Previous callback
 */
VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback callback);

/*---------------------------------------------------------------------------*
    PC-Specific Extensions
 *---------------------------------------------------------------------------*/

// Forward declarations for SDL types
typedef struct SDL_Window SDL_Window;
typedef void* SDL_GLContext;

/**
 * @brief Get SDL window (for GX rendering)
 * @return SDL_Window pointer
 */
SDL_Window* VIGetSDLWindow(void);

/**
 * @brief Get OpenGL context
 * @return SDL_GLContext
 */
SDL_GLContext VIGetGLContext(void);

/**
 * @brief Get current window size
 * @param width   Pointer to receive width
 * @param height  Pointer to receive height
 */
void VIGetWindowSize(int* width, int* height);

/*---------------------------------------------------------------------------*
    Internal Functions
 *---------------------------------------------------------------------------*/

void __VIInit(VITVMode mode);
void __VIResetSIIdle(void);
void __VIDisableDimming(void);
u32  __VISetDimmingCountLimit(u32 newLimit);
BOOL __VIResetRFIdle(void);
BOOL __VIResetDev0Idle(void);
BOOL __VIResetDev1Idle(void);
BOOL __VIResetDev2Idle(void);
BOOL __VIResetDev3Idle(void);
BOOL __VIResetDev4Idle(void);
BOOL __VIResetDev5Idle(void);
BOOL __VIResetDev6Idle(void);
BOOL __VIResetDev7Idle(void);
BOOL __VIResetDev8Idle(void);
BOOL __VIResetDev9Idle(void);

#ifdef __cplusplus
}
#endif

#endif // DOLPHIN_VI_H

