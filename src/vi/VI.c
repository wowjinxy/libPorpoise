/*---------------------------------------------------------------------------*
  VI.c - Video Interface
  
  ARCHITECTURAL DIFFERENCES: GC/Wii vs PC
  ========================================
  
  On GC/Wii (VI Hardware):
  ------------------------
  - Dedicated video hardware (VI registers)
  - Manages CRT/LCD display output
  - Frame buffer in main RAM (XFB - External Frame Buffer)
  - Hardware scans XFB and outputs to TV
  - Generates VBlank interrupts at 60Hz (NTSC) or 50Hz (PAL)
  - Configurable video modes (NTSC, PAL, progressive, interlaced)
  - Hardware handles field rendering for interlaced
  - VI registers control: position, size, format, filters
  
  On PC (SDL2 + OpenGL):
  ----------------------
  - SDL2 creates window and OpenGL context
  - Frame buffers provided by GX module (OpenGL textures/FBOs)
  - VSync via SDL_GL_SetSwapInterval
  - Window can be resized, fullscreen toggled
  - Display refresh rate queried from SDL
  - OpenGL handles buffer swapping
  - VBlank timing from SDL or manual timing
  
  WHY SDL2 + OpenGL:
  - SDL2 provides cross-platform window/input/timing
  - OpenGL for hardware-accelerated rendering (GX will use this)
  - Industry standard, widely supported
  - Easy to initialize and use
  
  WHAT WE PRESERVE:
  - Same API (VIInit, VIWaitForRetrace, VISetNextFrameBuffer, etc.)
  - Retrace callback concept (60Hz timing)
  - Frame buffer pointers (GX renders to them)
  - Black screen control
  
  WHAT'S DIFFERENT:
  - Creates actual window (640x480 default, resizable)
  - OpenGL context for GX rendering
  - VSync controlled via SDL
  - Retrace callbacks called from VBlank timing thread
  - Window can be closed by user (game should handle)
 *---------------------------------------------------------------------------*/

#include <dolphin/vi.h>
#include <dolphin/VIConfig.h>
#include <dolphin/os.h>
#include <dolphin/os/OSAlarm.h>
#include <dolphin/porpoise/Guard.h>
#ifdef LIBPORPOISE_BUILD_WIN64
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#include <simulator/glad/glad.h>
#include <string.h>
#include <stdlib.h>

// ACPC GX backend helper to switch from shader pipeline to fixed-function blit.
extern void pc_gx_prepare_for_present(void);

/*---------------------------------------------------------------------------*
    Internal State
 *---------------------------------------------------------------------------*/

static BOOL s_initialized = FALSE;
static BOOL s_black = TRUE;              // Start with black screen
static void* s_currentFB = NULL;         // Current frame buffer
static void* s_nextFB = NULL;            // Next frame buffer
static void* s_nextRightFB = NULL;       // Right eye FB (for 3D)
static BOOL s_3dMode = FALSE;            // 3D mode enabled

// Shadow/pending state. Per SDK docs, VI changes are latched by VIFlush and
// applied from the next field.
static BOOL s_pendingBlack = TRUE;
static BOOL s_pendingBlackDirty = FALSE;
static void* s_pendingNextFB = NULL;
static BOOL s_pendingNextFBDirty = FALSE;
static int s_pendingXfbWidth = 640;
static int s_pendingXfbHeight = 480;
static VITVMode s_pendingTvMode = VI_TVMODE_NTSC_INT;
static u32 s_pendingTvFormat = VI_NTSC;
static u32 s_pendingScanMode = VI_INTERLACE;
static BOOL s_pendingModeDirty = FALSE;
static struct {
    u16 xOrg;
    u16 yOrg;
    u16 width;
    u16 height;
} s_pendingPan = {0, 0, 640, 480};
static BOOL s_pendingPanDirty = FALSE;
static BOOL s_flushPending = FALSE;

// SDL2 Window and OpenGL context
static SDL_Window* s_window = NULL;
static SDL_GLContext s_glContext = NULL;
static int s_windowWidth = 640;
static int s_windowHeight = 480;
static int s_xfbWidth = 640;
static int s_xfbHeight = 480;
static GLuint s_xfbTexture = 0;

// Configuration
static VIConfig s_config;

// Video timing
static u32 s_retraceCount = 0;           // Number of VBlanks since init
static VITVMode s_tvMode = VI_TVMODE_NTSC_INT;
static u32 s_tvFormat = VI_NTSC;
static u32 s_scanMode = VI_INTERLACE;

// Callbacks
static VIRetraceCallback s_preRetraceCallback = NULL;
static VIRetraceCallback s_postRetraceCallback = NULL;

// VI register state (inspired by PPCArthur __VIEmuRegs)
// Track VI hardware register state for better compatibility
typedef struct {
    u16 dsp_cfg;        // VI_DSP_CFG - Display configuration
    u16 dsp_pos_u;      // VI_DSP_POS_U - Display position upper
    u16 dsp_pos_l;      // VI_DSP_POS_L - Display position lower
    u16 dsp_int0_u;     // VI_DSP_INT0_U - Interrupt 0 upper
    u16 dsp_int0_l;     // VI_DSP_INT0_L - Interrupt 0 lower
    u16 dsp_int1_u;     // VI_DSP_INT1_U - Interrupt 1 upper
    u16 dsp_int1_l;     // VI_DSP_INT1_L - Interrupt 1 lower
    u16 dsp_int2_u;     // VI_DSP_INT2_U - Interrupt 2 upper
    u16 dsp_int2_l;     // VI_DSP_INT2_L - Interrupt 2 lower
    u16 dsp_int3_u;     // VI_DSP_INT3_U - Interrupt 3 upper
    u16 dsp_int3_l;     // VI_DSP_INT3_L - Interrupt 3 lower
} VIRegisterState;

static VIRegisterState s_viRegs = {0};

// PI interrupt status (like PPCArthur __PIEmuRegs[PI_REG_INTSR])
// Track which interrupts are pending
static u32 s_piIntsr = 0;  // PI interrupt status register

// Timing alarm for VBlank simulation (like PPCArthur RetraceEmulator)
// Use OSAlarm instead of thread for more accurate timing
static OSAlarm s_retraceAlarm = {0};
static OSTime s_frameStart = 0;  // Frame start time for position calculation
static u32 s_field = 0;          // Current field (0=below, 1=above) for interlaced
static BOOL s_closeRequested = FALSE;

static void ProcessWindowEvents(void) {
    if (!s_initialized || !s_window) {
        return;
    }

    SDL_PumpEvents();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            OSReport("VI: Window close requested (SDL_QUIT)\n");
            s_closeRequested = TRUE;
        } else if (event.type == SDL_WINDOWEVENT) {
            switch (event.window.event) {
                case SDL_WINDOWEVENT_CLOSE:
                    OSReport("VI: Window close requested (SDL_WINDOWEVENT_CLOSE)\n");
                    s_closeRequested = TRUE;
                    break;
                case SDL_WINDOWEVENT_RESIZED:
                    s_windowWidth = event.window.data1;
                    s_windowHeight = event.window.data2;
                    OSReport("VI: Window resized to %dx%d\n", s_windowWidth, s_windowHeight);
                    break;
                case SDL_WINDOWEVENT_SHOWN:
                    OSReport("VI: Window shown\n");
                    break;
                case SDL_WINDOWEVENT_HIDDEN:
                    OSReport("VI: Window hidden\n");
                    break;
                default:
                    break;
            }
        }
    }

    // Match normal app behavior: closing the window terminates the process.
    if (s_closeRequested) {
        OSShutdownSystem();
    }
}

/*---------------------------------------------------------------------------*
  Name:         RetraceEmulator

  Description:  Alarm handler for fake 60Hz VBlank interrupt simulation.
                Inspired by PPCArthur's RetraceEmulator().
                Called by OSAlarm at display refresh rate.

  Arguments:    alarm   60Hz timer alarm
                context Current context

  Returns:      None
 *---------------------------------------------------------------------------*/
static void RetraceEmulator(OSAlarm* alarm, OSContext* context) {
    (void)alarm;
    (void)context;

    if (s_flushPending) {
        if (s_pendingModeDirty) {
            s_tvMode = s_pendingTvMode;
            s_tvFormat = s_pendingTvFormat;
            s_scanMode = s_pendingScanMode;
            s_xfbWidth = s_pendingXfbWidth;
            s_xfbHeight = s_pendingXfbHeight;
            s_pendingModeDirty = FALSE;
        }

        // Pan settings are tracked for API compatibility but not used by the
        // PC presenter yet.
        s_pendingPanDirty = FALSE;

        if (s_pendingBlackDirty) {
            s_black = s_pendingBlack;
            s_pendingBlackDirty = FALSE;
        }
        if (s_pendingNextFBDirty) {
            s_nextFB = s_pendingNextFB;
            s_pendingNextFBDirty = FALSE;
        }

        s_flushPending = FALSE;
    }
    
    // Toggle field for interlaced mode (like PPCArthur)
    s_field ^= 1;
    
    // Set interrupt status based on field
    // On original hardware, VI generates interrupts at field boundaries
    if (s_field == 0) {
        // Field below - set INT0
        s_viRegs.dsp_int0_u |= 0x0001;  // INT0_MASK
        s_piIntsr |= 0x0001;  // PI_INTSR_REG_VIINT_MASK
    } else {
        // Field above - set INT1 (for next field)
        s_viRegs.dsp_int1_u |= 0x0001;  // INT1_MASK
        s_piIntsr |= 0x0001;  // PI_INTSR_REG_VIINT_MASK
        
        // Store frame start time for position calculation
        s_frameStart = OSGetTime();
    }
    
    // Pre-retrace callback (if enabled)
    if (s_config.enableCallbacks && s_preRetraceCallback) {
        s_preRetraceCallback(s_retraceCount);
    }
    
    // Swap buffers (simulate VI hardware updating current FB)
    if (s_nextFB) {
        s_currentFB = s_nextFB;
    }
    
    // Increment retrace count
    s_retraceCount++;
    
    // Post-retrace callback (if enabled)
    if (s_config.enableCallbacks && s_postRetraceCallback) {
        s_postRetraceCallback(s_retraceCount);
    }
    
    // Note: On original hardware, this would trigger __OSDispatchInterrupt()
    // On PC, we just call callbacks directly
}

/*---------------------------------------------------------------------------*
  Name:         InitVIRetrace

  Description:  Initialize 60Hz timer interrupt simulation.
                Inspired by PPCArthur's InitVIRetrace().

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
static void InitVIRetrace(void) {
    // Initialize alarm structure (must call before OSSetPeriodicAlarm)
    OSCreateAlarm(&s_retraceAlarm);
    
    // Calculate frame time based on TV mode
    OSTime frameTime;
    if (s_config.tvMode == 1) {
        frameTime = OSMicrosecondsToTicks(1000000 / 50);  // PAL: 50Hz (20ms)
    } else if (s_config.fpsCap > 0 && s_config.vsync == 0) {
        frameTime = OSMicrosecondsToTicks(1000000 / s_config.fpsCap);
    } else {
        frameTime = OSMicrosecondsToTicks(1000000 / 60);  // NTSC: 60Hz (16.67ms)
    }
    
    // Set up periodic alarm for VBlank simulation
    // Like PPCArthur: OSSetPeriodicAlarm(&alarm, OSGetTime(), frameTime, RetraceEmulator)
    OSSetPeriodicAlarm(&s_retraceAlarm, OSGetTime(), frameTime, RetraceEmulator);
    s_frameStart = OSGetTime();
    
    OSReport("VI: Retrace alarm initialized (%s Hz)\n", 
             s_config.tvMode == 1 ? "50" : "60");
}

/*---------------------------------------------------------------------------*
  Name:         VIInit

  Description:  Initialize the Video Interface.
                
                On GC/Wii: Configures VI hardware, sets up interrupts
                On PC: Sets up retrace simulation thread

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void VIInit(void) {
    if (s_initialized) {
        return;
    }
    
    OSReport("VI: Initializing video interface...\n");
    
    // Load configuration from vi_config.ini
    VILoadConfig(&s_config);
    
    // Apply config values
    s_windowWidth = s_config.windowWidth;
    s_windowHeight = s_config.windowHeight;
    s_xfbWidth = 640;
    s_xfbHeight = 480;
    s_xfbTexture = 0;
    
    // Ensure SDL is initialized (required before subsystems)
    // SDL_InitSubSystem can work without SDL_Init, but it's safer to init first
    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            OSReport("VI: ERROR - Failed to initialize SDL: %s\n", SDL_GetError());
            OSPanic(__FILE__, __LINE__, "VI: SDL initialization failed");
            return;
        }
        OSReport("VI: SDL initialized\n");
    } else {
        // SDL already initialized, just ensure VIDEO subsystem is active
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
            OSReport("VI: ERROR - Failed to initialize SDL video subsystem: %s\n", SDL_GetError());
            OSPanic(__FILE__, __LINE__, "VI: SDL video subsystem initialization failed");
            return;
        }
    }
    
    // Set OpenGL attributes from config
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, s_config.openglMajor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, s_config.openglMinor);
    // Use compatibility profile to support fixed-function pipeline (glMatrixMode, glVertexPointer, etc.)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    
    // Set MSAA if requested
    if (s_config.msaaSamples > 0) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, s_config.msaaSamples);
    }
    
    // Determine window flags
    u32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    
    if (s_config.fullscreen) {
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    } else {
        windowFlags |= SDL_WINDOW_RESIZABLE;
        if (s_config.maximized) {
            windowFlags |= SDL_WINDOW_MAXIMIZED;
        }
    }
    
    // Create window
    OSReport("VI: Creating SDL window (%dx%d)...\n", s_windowWidth, s_windowHeight);
    s_window = SDL_CreateWindow(
        s_config.windowTitle,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        s_windowWidth,
        s_windowHeight,
        windowFlags
    );
    
    if (!s_window) {
        OSReport("VI: ERROR - Failed to create window: %s\n", SDL_GetError());
        OSPanic(__FILE__, __LINE__, "VI: Window creation failed - %s", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }
    
    OSReport("VI: Window created successfully\n");
    
    // Explicitly show the window (though SDL_WINDOW_SHOWN should do this)
    SDL_ShowWindow(s_window);
    
    // Restore window if it's minimized
    SDL_RestoreWindow(s_window);
    
    // Raise window to front
    SDL_RaiseWindow(s_window);
    
    // Process one event to ensure window appears
    SDL_Event event;
    SDL_PumpEvents();
    
    // Create OpenGL context
    OSReport("VI: Creating OpenGL context...\n");
    s_glContext = SDL_GL_CreateContext(s_window);
    if (!s_glContext) {
        OSReport("VI: ERROR - Failed to create OpenGL context: %s\n", SDL_GetError());
        OSReport("VI: Make sure you have a graphics driver installed\n");
        OSPanic(__FILE__, __LINE__, "VI: OpenGL context creation failed - %s", SDL_GetError());
        SDL_DestroyWindow(s_window);
        s_window = NULL;
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }
    
    OSReport("VI: OpenGL context created successfully\n");
    
    
    // Set VSync from config
    if (SDL_GL_SetSwapInterval(s_config.vsync) < 0) {
        OSReport("VI: Warning: Failed to set VSync mode %d\n", s_config.vsync);
        // Try fallback to VSync on
        SDL_GL_SetSwapInterval(1);
    }
    
    OSReport("VI: SDL2 window created (%dx%d) %s\n", 
             s_windowWidth, s_windowHeight,
             s_config.fullscreen ? "fullscreen" : "windowed");
    OSReport("VI: OpenGL %d.%d context created\n", 
             s_config.openglMajor, s_config.openglMinor);
    OSReport("VI: VSync: %s\n", 
             s_config.vsync == 1 ? "On" : (s_config.vsync == -1 ? "Adaptive" : "Off"));
    
    // Initialize state
    s_black = TRUE;
    s_currentFB = NULL;
    s_nextFB = NULL;
    s_nextRightFB = NULL;
    s_3dMode = FALSE;
    s_retraceCount = 0;
    // Preserve mode selected by __VIInit (if used) and derive format fields.
    s_tvFormat = ((u32)s_tvMode >> 2) & 0xF;
    s_scanMode = ((u32)s_tvMode) & 0x3;
    s_preRetraceCallback = NULL;
    s_postRetraceCallback = NULL;

    s_pendingBlack = s_black;
    s_pendingBlackDirty = FALSE;
    s_pendingNextFB = s_nextFB;
    s_pendingNextFBDirty = FALSE;
    s_pendingXfbWidth = s_xfbWidth;
    s_pendingXfbHeight = s_xfbHeight;
    s_pendingTvMode = s_tvMode;
    s_pendingTvFormat = s_tvFormat;
    s_pendingScanMode = s_scanMode;
    s_pendingModeDirty = FALSE;
    s_pendingPan.xOrg = 0;
    s_pendingPan.yOrg = 0;
    s_pendingPan.width = (u16)s_xfbWidth;
    s_pendingPan.height = (u16)s_xfbHeight;
    s_pendingPanDirty = FALSE;
    s_flushPending = FALSE;
    
    // Initialize VI register state (like PPCArthur)
    memset(&s_viRegs, 0, sizeof(s_viRegs));
    s_piIntsr = 0;
    s_field = 0;
    s_frameStart = 0;
    
    // Initialize retrace alarm (like PPCArthur InitVIRetrace)
    // Check if VI is enabled (DSP_CFG_ENB bit)
    // On original hardware, retrace timer only starts when VI is enabled
    InitVIRetrace();
    
    s_initialized = TRUE;
    OSReport("VI: Video interface initialized\n");
    OSReport("VI: TV Mode: NTSC 60Hz\n");
    OSReport("VI: Window ready for rendering\n");
}

/*---------------------------------------------------------------------------*
  Name:         __VIInit

  Description:  Internal VI initialization with specific mode.

  Arguments:    mode  TV mode to use

  Returns:      None
 *---------------------------------------------------------------------------*/
void __VIInit(VITVMode mode) {
    s_tvMode = mode;
    
    // Extract format and scan mode from TV mode
    s_tvFormat = (mode >> 2) & 0xF;
    s_scanMode = mode & 0x3;
    
    VIInit();
}

/*---------------------------------------------------------------------------*
  Name:         VIWaitForRetrace

  Description:  Wait for next vertical retrace.
                
                On GC/Wii: Blocks until VBlank interrupt
                On PC: Sleeps for one frame time

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void VIWaitForRetrace(void) {
    PP_GUARD_VOID(s_initialized, "VIInit must be called first");
    
    u32 currentCount = s_retraceCount;
    
    // Wait until retrace count increments
    while (s_retraceCount == currentCount) {
        ProcessWindowEvents();
        OSSleepTicks(OSMillisecondsToTicks(1));
    }
}

/*---------------------------------------------------------------------------*
  Name:         VIFlush

  Description:  Flush VI configuration to hardware. On PC, this swaps the
                OpenGL back buffer to display.
                
                On GC/Wii: Writes shadow registers to VI
                On PC: Swaps SDL GL buffers

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void VIFlush(void) {
    PP_GUARD_VOID(s_initialized, "VIInit must be called first");
    PP_GUARD_VOID(s_window != NULL, "window not initialized");
    ProcessWindowEvents();

    // Match SDK semantics: VI state changes are latched by VIFlush and become
    // visible from a subsequent field boundary.
    s_flushPending = TRUE;

    // Don't swap here - swap is handled by DEMOSwapBuffers() after rendering.
    // This prevents double-swapping which causes flashing.
}

/*---------------------------------------------------------------------------*
  Name:         VISetNextFrameBuffer

  Description:  Set the next frame buffer to be displayed.

  Arguments:    fb  Pointer to frame buffer

  Returns:      None
 *---------------------------------------------------------------------------*/
void VISetNextFrameBuffer(void* fb) {
    s_pendingNextFB = fb;
    s_pendingNextFBDirty = TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         VIGetNextFrameBuffer

  Description:  Get the next frame buffer pointer.

  Arguments:    None

  Returns:      Pointer to next frame buffer
 *---------------------------------------------------------------------------*/
void* VIGetNextFrameBuffer(void) {
    if (s_pendingNextFBDirty) {
        return s_pendingNextFB;
    }
    return s_nextFB;
}

/*---------------------------------------------------------------------------*
  Name:         VIGetCurrentFrameBuffer

  Description:  Get the current frame buffer pointer.

  Arguments:    None

  Returns:      Pointer to current frame buffer
 *---------------------------------------------------------------------------*/
void* VIGetCurrentFrameBuffer(void) {
    return s_currentFB;
}

/*---------------------------------------------------------------------------*
  Name:         VISetNextRightFrameBuffer

  Description:  Set right eye frame buffer for 3D mode.

  Arguments:    fb  Pointer to right frame buffer

  Returns:      None
 *---------------------------------------------------------------------------*/
void VISetNextRightFrameBuffer(void* fb) {
    s_nextRightFB = fb;
}

/*---------------------------------------------------------------------------*
  Name:         VISetBlack

  Description:  Enable or disable black screen.
                
                On GC/Wii: Hardware blanks video output
                On PC: Just sets flag (game checks this)

  Arguments:    black  TRUE for black screen, FALSE for normal

  Returns:      None
 *---------------------------------------------------------------------------*/
void VISetBlack(BOOL black) {
    s_pendingBlack = black;
    s_pendingBlackDirty = TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         VISet3D

  Description:  Enable or disable 3D stereoscopic mode.

  Arguments:    threeD  TRUE for 3D, FALSE for 2D

  Returns:      None
 *---------------------------------------------------------------------------*/
void VISet3D(BOOL threeD) {
    s_3dMode = threeD;
}

/*---------------------------------------------------------------------------*
  Name:         VIConfigure

  Description:  Configure video mode from render mode object.
                
                On GC/Wii: Configures VI registers from mode object
                On PC: Extracts and stores TV mode and scan mode from render mode

  Arguments:    rm  Pointer to GXRenderModeObj

  Returns:      None
 *---------------------------------------------------------------------------*/
void VIConfigure(const GXRenderModeObj* rm) {
    PP_GUARD_VOID(rm != NULL, "null pointer");

    if (rm->fbWidth > 0) {
        s_pendingXfbWidth = (int)VIPadFrameBufferWidth(rm->fbWidth);
    }
    if (rm->xfbHeight > 0) {
        s_pendingXfbHeight = (int)rm->xfbHeight;
    }
    s_pendingTvMode = (VITVMode)rm->viTVmode;
    s_pendingTvFormat = ((u32)rm->viTVmode >> 2) & 0xF;
    s_pendingScanMode = ((u32)rm->viTVmode) & 0x3;
    s_pendingModeDirty = TRUE;

    // VIConfigure resets pan defaults to mode values; callers can override
    // again with VIConfigurePan before VIFlush.
    s_pendingPan.xOrg = rm->viXOrigin;
    s_pendingPan.yOrg = rm->viYOrigin;
    s_pendingPan.width = rm->viWidth;
    s_pendingPan.height = rm->viHeight;
    s_pendingPanDirty = TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         VIConfigurePan

  Description:  Configure panning (screen position/size).

  Arguments:    xOrg    X origin
                yOrg    Y origin
                width   Width
                height  Height

  Returns:      None
 *---------------------------------------------------------------------------*/
void VIConfigurePan(u16 xOrg, u16 yOrg, u16 width, u16 height) {
    s_pendingPan.xOrg = xOrg;
    s_pendingPan.yOrg = yOrg;
    s_pendingPan.width = width;
    s_pendingPan.height = height;
    s_pendingPanDirty = TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         VIGetRetraceCount

  Description:  Get number of vertical retraces since initialization.

  Arguments:    None

  Returns:      Retrace count
 *---------------------------------------------------------------------------*/
u32 VIGetRetraceCount(void) {
    return s_retraceCount;
}

/*---------------------------------------------------------------------------*
  Name:         VIGetNextField

  Description:  Get next field type (for interlaced modes).

  Arguments:    None

  Returns:      VI_FIELD_ABOVE or VI_FIELD_BELOW
 *---------------------------------------------------------------------------*/
u32 VIGetNextField(void) {
    // Return current field (like PPCArthur)
    // Field toggles each retrace for interlaced mode
    return s_field ? VI_FIELD_ABOVE : VI_FIELD_BELOW;
}

/*---------------------------------------------------------------------------*
  Name:         VIGetCurrentLine

  Description:  Get current scan line being drawn.

  Arguments:    None

  Returns:      Current line (0 on PC)
 *---------------------------------------------------------------------------*/
u32 VIGetCurrentLine(void) {
    // Calculate current scan line based on time since frame start
    // Inspired by PPCArthur's VI_DSP_POS calculation
    if (s_frameStart == 0) {
        return 0;
    }
    
    OSTime elapsed = OSGetTime() - s_frameStart;
    u32 nanoseconds = (u32)OSTicksToNanoseconds(elapsed);
    
    // Calculate line based on TV mode
    // NTSC: 262.5 lines per field, ~74ns per dot, ~429 dots per half-line
    u32 dotsPerHalfLine = 429;
    u32 dotsPerField = 262 * 2 * dotsPerHalfLine;  // 262 lines * 2 fields * half-line
    u32 dot = (nanoseconds / 74) % dotsPerField;
    u32 line = (dot / (2 * dotsPerHalfLine)) + 1;  // +1 because line 0 is reserved
    
    // Clamp to valid range
    if (line > 262) {
        line = 262;
    }
    
    return line;
}

/*---------------------------------------------------------------------------*
  Name:         VIGetTvFormat

  Description:  Get TV format (NTSC/PAL/MPAL/EURGB60).

  Arguments:    None

  Returns:      TV format constant
 *---------------------------------------------------------------------------*/
u32 VIGetTvFormat(void) {
    return s_tvFormat;
}

/*---------------------------------------------------------------------------*
  Name:         VIGetScanMode

  Description:  Get scan mode (interlace/progressive).

  Arguments:    None

  Returns:      Scan mode constant
 *---------------------------------------------------------------------------*/
u32 VIGetScanMode(void) {
    return s_scanMode;
}

/*---------------------------------------------------------------------------*
  Name:         VIGetDTVStatus

  Description:  Get digital TV status.

  Arguments:    None

  Returns:      DTV status (0 on PC)
 *---------------------------------------------------------------------------*/
u32 VIGetDTVStatus(void) {
    return 0;  // No DTV hardware on PC
}

/*---------------------------------------------------------------------------*
  Name:         VISetPreRetraceCallback

  Description:  Set callback to be called before retrace.

  Arguments:    callback  Callback function

  Returns:      Previous callback
 *---------------------------------------------------------------------------*/
VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback callback) {
    VIRetraceCallback prev = s_preRetraceCallback;
    s_preRetraceCallback = callback;
    return prev;
}

/*---------------------------------------------------------------------------*
  Name:         VISetPostRetraceCallback

  Description:  Set callback to be called after retrace.

  Arguments:    callback  Callback function

  Returns:      Previous callback
 *---------------------------------------------------------------------------*/
VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback callback) {
    VIRetraceCallback prev = s_postRetraceCallback;
    s_postRetraceCallback = callback;
    return prev;
}

/*---------------------------------------------------------------------------*
  Name:         __VIResetSIIdle

  Description:  Reset SI idle timer (for dimming).
                Called when controller input detected.

  Arguments:    None

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL __VIResetSIIdle(void) {
    /* Dimming not implemented on PC. */
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         __VIDisableDimming

  Description:  Disable screen dimming.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void __VIDisableDimming(void) {
    /* Dimming not implemented on PC. */
}

/*---------------------------------------------------------------------------*
  Name:         __VISetDimmingCountLimit

  Description:  Set dimming timer limit.

  Arguments:    newLimit  New count limit

  Returns:      Previous limit (0 on PC)
 *---------------------------------------------------------------------------*/
u32 __VISetDimmingCountLimit(u32 newLimit) {
    (void)newLimit;
    return 0;
}

/*---------------------------------------------------------------------------*
  Name:         __VIResetRFIdle, __VIResetDevXIdle

  Description:  Reset various device idle timers.
                Used for power management/dimming on Wii.

  Arguments:    None

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL __VIResetRFIdle(void) { return TRUE; }
BOOL __VIResetDev0Idle(void) { return TRUE; }
BOOL __VIResetDev1Idle(void) { return TRUE; }
BOOL __VIResetDev2Idle(void) { return TRUE; }
BOOL __VIResetDev3Idle(void) { return TRUE; }
BOOL __VIResetDev4Idle(void) { return TRUE; }
BOOL __VIResetDev5Idle(void) { return TRUE; }
BOOL __VIResetDev6Idle(void) { return TRUE; }
BOOL __VIResetDev7Idle(void) { return TRUE; }
BOOL __VIResetDev8Idle(void) { return TRUE; }
BOOL __VIResetDev9Idle(void) { return TRUE; }

/*---------------------------------------------------------------------------*
  Name:         VIGetSDLWindow

  Description:  PC-specific: Get SDL window handle for rendering.
                GX module will use this to set up OpenGL rendering.

  Arguments:    None

  Returns:      SDL_Window pointer
 *---------------------------------------------------------------------------*/
SDL_Window* VIGetSDLWindow(void) {
    return s_window;
}

/*---------------------------------------------------------------------------*
  Name:         VIGetGLContext

  Description:  PC-specific: Get OpenGL context.

  Arguments:    None

  Returns:      SDL_GLContext
 *---------------------------------------------------------------------------*/
SDL_GLContext VIGetGLContext(void) {
    return s_glContext;
}

/*---------------------------------------------------------------------------*
  Name:         VIGetWindowSize

  Description:  PC-specific: Get current window dimensions.

  Arguments:    width   Pointer to receive width
                height  Pointer to receive height

  Returns:      None
 *---------------------------------------------------------------------------*/
void VIGetWindowSize(int* width, int* height) {
    if (width) *width = s_windowWidth;
    if (height) *height = s_windowHeight;
}

/*---------------------------------------------------------------------------*
  Name:         VIMakeContextCurrent
  
  Description:  PC-specific: Make OpenGL context current for this thread.
                This should be called before rendering.
  
  Arguments:    None
  
  Returns:      None
 *---------------------------------------------------------------------------*/
void VIMakeContextCurrent(void) {
    if (s_window && s_glContext) {
        if (SDL_GL_MakeCurrent(s_window, s_glContext) != 0) {
            OSReport("[VIMakeContextCurrent] ERROR: SDL_GL_MakeCurrent failed: %s\n", SDL_GetError());
        }
    }
}

BOOL VIIsRenderSuspended(void) {
    if (!s_window) {
        return FALSE;
    }

    const Uint32 flags = SDL_GetWindowFlags(s_window);
    if ((flags & SDL_WINDOW_MINIMIZED) || (flags & SDL_WINDOW_HIDDEN)) {
        return TRUE;
    }

    // Treat unfocused windows as suspended to avoid tab-away GPU stalls.
    if ((flags & SDL_WINDOW_INPUT_FOCUS) == 0) {
        return TRUE;
    }

    if (s_windowWidth <= 0 || s_windowHeight <= 0) {
        return TRUE;
    }

    return FALSE;
}

static void VIPresentCurrentFrameBuffer(void) {
    if (s_black || !s_currentFB || s_xfbWidth <= 0 || s_xfbHeight <= 0) {
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    if (s_xfbTexture == 0) {
        glGenTextures(1, &s_xfbTexture);
        glBindTexture(GL_TEXTURE_2D, s_xfbTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    pc_gx_prepare_for_present();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, s_windowWidth, s_windowHeight);
    // Always reset the backbuffer before blitting XFB to avoid stale pixels/ghost trails.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, s_xfbTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, s_xfbWidth, s_xfbHeight, 0,
                 GL_RGB, GL_UNSIGNED_SHORT_5_6_5, s_currentFB);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f,  1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f,  1.0f);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

/*---------------------------------------------------------------------------*
  Name:         VISwapBuffers
  
  Description:  PC-specific: Swap OpenGL buffers to display rendered frame.
                This should be called after rendering is complete.
  
  Arguments:    None
  
  Returns:      None
 *---------------------------------------------------------------------------*/
void VISwapBuffers(void) {
    PP_GUARD_VOID(s_initialized, "VIInit must be called first");
    PP_GUARD_VOID(s_window != NULL, "window not initialized");
    PP_GUARD_VOID(s_glContext != NULL, "GL context not initialized");

    if (VIIsRenderSuspended()) {
        return;
    }

    // Ensure context is current before swapping
    if (SDL_GL_MakeCurrent(s_window, s_glContext) != 0) {
        OSReport("[VISwapBuffers] ERROR: SDL_GL_MakeCurrent failed: %s\n", SDL_GetError());
        return;
    }
    VIPresentCurrentFrameBuffer();
    SDL_GL_SwapWindow(s_window);
}
