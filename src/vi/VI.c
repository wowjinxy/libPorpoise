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
#include <SDL.h>
#include <string.h>

/*---------------------------------------------------------------------------*
    Internal State
 *---------------------------------------------------------------------------*/

static BOOL s_initialized = FALSE;
static BOOL s_black = TRUE;              // Start with black screen
static void* s_currentFB = NULL;         // Current frame buffer
static void* s_nextFB = NULL;            // Next frame buffer
static void* s_nextRightFB = NULL;       // Right eye FB (for 3D)
static BOOL s_3dMode = FALSE;            // 3D mode enabled

// SDL2 Window and OpenGL context
static SDL_Window* s_window = NULL;
static SDL_GLContext s_glContext = NULL;
static int s_windowWidth = 640;
static int s_windowHeight = 480;

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

// Timing thread for VBlank simulation
#ifdef _WIN32
static HANDLE s_retraceThread = NULL;
static volatile BOOL s_retraceRunning = FALSE;
#else
static pthread_t s_retraceThread;
static volatile BOOL s_retraceRunning = FALSE;
#endif

/*---------------------------------------------------------------------------*
  Name:         RetraceThread

  Description:  Background thread that simulates VBlank interrupts.
                Calls retrace callbacks at display refresh rate.

  Arguments:    arg  Unused

  Returns:      0 (thread return)
 *---------------------------------------------------------------------------*/
#ifdef _WIN32
static DWORD WINAPI RetraceThread(LPVOID arg)
#else
static void* RetraceThread(void* arg)
#endif
{
    (void)arg;
    
    // Calculate frame time based on TV mode from config
    u32 frameTimeMs;
    if (s_config.tvMode == 1) {
        frameTimeMs = 20;  // PAL: 50Hz
    } else if (s_config.fpsCap > 0 && s_config.vsync == 0) {
        frameTimeMs = 1000 / s_config.fpsCap;  // Custom FPS cap
    } else {
        frameTimeMs = 16;  // NTSC: 60Hz (default)
    }
    
    while (s_retraceRunning) {
        // Sleep for one frame
        OSSleepTicks(OSMillisecondsToTicks(frameTimeMs));
        
        // Pre-retrace callback (if enabled in config)
        if (s_config.enableCallbacks && s_preRetraceCallback) {
            s_preRetraceCallback(s_retraceCount);
        }
        
        // Swap buffers (simulate VI hardware updating current FB)
        if (s_nextFB) {
            s_currentFB = s_nextFB;
        }
        
        // Increment retrace count
        s_retraceCount++;
        
        // Post-retrace callback (if enabled in config)
        if (s_config.enableCallbacks && s_postRetraceCallback) {
            s_postRetraceCallback(s_retraceCount);
        }
    }
    
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
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
    
    // Initialize SDL2 video subsystem
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
        OSReport("VI: Failed to initialize SDL video: %s\n", SDL_GetError());
        return;
    }
    
    // Set OpenGL attributes from config
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, s_config.openglMajor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, s_config.openglMinor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
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
    s_window = SDL_CreateWindow(
        s_config.windowTitle,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        s_windowWidth,
        s_windowHeight,
        windowFlags
    );
    
    if (!s_window) {
        OSReport("VI: Failed to create window: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }
    
    // Create OpenGL context
    s_glContext = SDL_GL_CreateContext(s_window);
    if (!s_glContext) {
        OSReport("VI: Failed to create OpenGL context: %s\n", SDL_GetError());
        SDL_DestroyWindow(s_window);
        s_window = NULL;
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }
    
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
    s_tvMode = VI_TVMODE_NTSC_INT;
    s_tvFormat = VI_NTSC;
    s_scanMode = VI_INTERLACE;
    s_preRetraceCallback = NULL;
    s_postRetraceCallback = NULL;
    
    // Start retrace simulation thread
    s_retraceRunning = TRUE;
    
#ifdef _WIN32
    s_retraceThread = CreateThread(NULL, 0, RetraceThread, NULL, 0, NULL);
    if (!s_retraceThread) {
        OSReport("VI: Failed to create retrace thread\n");
        s_retraceRunning = FALSE;
    }
#else
    if (pthread_create(&s_retraceThread, NULL, RetraceThread, NULL) != 0) {
        OSReport("VI: Failed to create retrace thread\n");
        s_retraceRunning = FALSE;
    }
#endif
    
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
    if (!s_initialized) {
        return;
    }
    
    u32 currentCount = s_retraceCount;
    
    // Wait until retrace count increments
    while (s_retraceCount == currentCount) {
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
    if (!s_initialized || !s_window) {
        return;
    }
    
    // Process SDL events (window close, resize, etc.)
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            OSReport("VI: Window close requested\n");
        } else if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                s_windowWidth = event.window.data1;
                s_windowHeight = event.window.data2;
                OSReport("VI: Window resized to %dx%d\n", s_windowWidth, s_windowHeight);
            }
        }
    }
    
    // Swap OpenGL buffers to display rendered frame
    if (s_glContext) {
        SDL_GL_SwapWindow(s_window);
    }
}

/*---------------------------------------------------------------------------*
  Name:         VISetNextFrameBuffer

  Description:  Set the next frame buffer to be displayed.

  Arguments:    fb  Pointer to frame buffer

  Returns:      None
 *---------------------------------------------------------------------------*/
void VISetNextFrameBuffer(void* fb) {
    s_nextFB = fb;
}

/*---------------------------------------------------------------------------*
  Name:         VIGetNextFrameBuffer

  Description:  Get the next frame buffer pointer.

  Arguments:    None

  Returns:      Pointer to next frame buffer
 *---------------------------------------------------------------------------*/
void* VIGetNextFrameBuffer(void) {
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
    s_black = black;
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
                On PC: Stores mode information

  Arguments:    rm  Pointer to GXRenderModeObj

  Returns:      None
 *---------------------------------------------------------------------------*/
void VIConfigure(const GXRenderModeObj* rm) {
    (void)rm;
    
    /* On PC, the render mode is handled by GX module.
     * VI just needs to know it was configured.
     */
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
    (void)xOrg;
    (void)yOrg;
    (void)width;
    (void)height;
    
    /* Panning not implemented on PC. */
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
    // Alternate fields for interlaced
    return (s_retraceCount & 1) ? VI_FIELD_BELOW : VI_FIELD_ABOVE;
}

/*---------------------------------------------------------------------------*
  Name:         VIGetCurrentLine

  Description:  Get current scan line being drawn.

  Arguments:    None

  Returns:      Current line (0 on PC)
 *---------------------------------------------------------------------------*/
u32 VIGetCurrentLine(void) {
    return 0;  // Not tracked on PC
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

