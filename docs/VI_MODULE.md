# VI (Video Interface) Module - Complete Documentation

## Overview

The VI (Video Interface) module manages display output and frame buffer presentation. On GameCube/Wii, this controlled dedicated video hardware that scanned frame buffers and output to TV. On PC with libPorpoise, VI creates an SDL2 window with OpenGL context for hardware-accelerated rendering.

---

## Architecture Comparison

### GameCube/Wii Hardware

**VI Registers** (Memory-mapped hardware):
- Control CRT/LCD display output directly
- Scan frame buffer (XFB) and output to TV
- Generate VBlank interrupts at 60Hz (NTSC) or 50Hz (PAL)
- Configure: resolution, position, interlace, color format
- Support multiple TV standards (NTSC, PAL, MPAL, progressive)

**Frame Buffer (XFB)**:
- Stored in main RAM (24MB)
- Format: YUV 4:2:2 (2 bytes per pixel)
- Hardware scans and converts to analog video
- Double/triple buffering for smooth animation

**VBlank Interrupt**:
- Hardware interrupt every 16.67ms (NTSC) or 20ms (PAL)
- Used for game loop timing
- Pre/post retrace callbacks
- Critical for synchronization

### PC Implementation (SDL2 + OpenGL)

**SDL2 Window**:
- Creates actual OS window (640x480 default, resizable)
- Cross-platform (Windows, Linux, macOS)
- Event handling (resize, close, fullscreen)
- VSync via SDL_GL_SetSwapInterval

**OpenGL 3.3 Core Context**:
- Hardware-accelerated rendering
- Double-buffered (front/back buffer)
- GX module will render to this
- Modern programmable pipeline (shaders)

**VBlank Simulation**:
- Background thread runs at 60Hz
- Sleeps for 16ms between frames
- Calls pre/post retrace callbacks
- Increments retrace counter

---

## Public API Functions

### Initialization

#### `void VIInit(void)`

**Purpose**: Initialize video system and create display window

**GameCube/Wii**:
- Configures VI hardware registers
- Sets up VBlank interrupt handler
- Initializes timing parameters
- Starts in black screen mode

**PC Implementation**:
- Initializes SDL2 video subsystem
- Creates 640x480 resizable window
- Creates OpenGL 3.3 Core context
- Enables VSync (locks to display refresh)
- Starts 60Hz retrace simulation thread

**Usage**:
```c
VIInit();  // Call once at startup
```

---

#### `void __VIInit(VITVMode mode)`

**Purpose**: Internal initialization with specific TV mode

**GameCube/Wii**:
- Programs VI registers for NTSC/PAL/MPAL/progressive
- Sets refresh rate (60Hz or 50Hz)
- Configures interlace vs progressive scan

**PC Implementation**:
- Extracts TV format and scan mode from enum
- Stores for VIGetTvFormat/VIGetScanMode queries
- Calls VIInit()

**Modes**:
- `VI_TVMODE_NTSC_INT` - NTSC interlaced (60Hz)
- `VI_TVMODE_NTSC_PROG` - NTSC progressive (480p, 60Hz)
- `VI_TVMODE_PAL_INT` - PAL interlaced (50Hz)
- `VI_TVMODE_EURGB60_PROG` - European RGB 60Hz progressive

---

### Synchronization

#### `void VIWaitForRetrace(void)`

**Purpose**: Block until next vertical blanking interval

**GameCube/Wii**:
- Suspends thread until VBlank interrupt
- Hardware-precise 16.67ms timing
- Used for game loop synchronization

**PC Implementation**:
- Polls retrace counter
- Sleeps 1ms between polls
- Returns when counter increments (next frame)

**Usage**:
```c
while (running) {
    // Update game logic
    UpdateGame();
    
    // Render frame
    GXDraw();
    
    // Wait for VBlank
    VIWaitForRetrace();
    
    // Swap buffers
    VIFlush();
}
```

**Performance Note**: On PC, VSync is preferred over busy-wait. Game should use VIFlush() for actual buffer swap.

---

#### `void VIFlush(void)`

**Purpose**: Flush VI configuration and swap display buffers

**GameCube/Wii**:
- Writes shadow registers to VI hardware
- Configuration takes effect on next VBlank
- Does NOT swap buffers (hardware does that)

**PC Implementation**:
- **Swaps OpenGL front/back buffers** (displays rendered frame!)
- Processes SDL events (window resize, close)
- VSync blocks until display refresh

**Usage**:
```c
// Render to back buffer
GXBegin(GX_TRIANGLES);
// ... draw commands ...
GXEnd();

// Display it
VIFlush();  // Shows frame on screen!
```

**Critical**: On PC, VIFlush() is the actual buffer swap. Games MUST call this every frame to display anything.

---

### Frame Buffer Management

#### `void VISetNextFrameBuffer(void* fb)`

**Purpose**: Set frame buffer to be displayed on next VBlank

**GameCube/Wii**:
- Stores pointer to XFB in RAM
- VI hardware scans this buffer and outputs to TV
- Typically double or triple buffered

**PC Implementation**:
- Stores pointer (informational only)
- Retrace thread copies to "current" on VBlank
- Actual rendering handled by OpenGL

**Usage**:
```c
void* framebuffer = GXAllocFrameBuffer();
GXRenderToBuffer(framebuffer);
VISetNextFrameBuffer(framebuffer);
VIWaitForRetrace();  // VI switches to this FB
```

---

#### `void* VIGetNextFrameBuffer(void)`

**Purpose**: Get pointer to next frame buffer

**Returns**: Pointer set by VISetNextFrameBuffer()

---

#### `void* VIGetCurrentFrameBuffer(void)`

**Purpose**: Get pointer to currently displayed frame buffer

**Returns**: Buffer being scanned by VI hardware (updated on VBlank)

---

#### `void VISetNextRightFrameBuffer(void* fb)`

**Purpose**: Set right-eye frame buffer for stereoscopic 3D

**GameCube/Wii**: Used for 3D mode (rare)

**PC Implementation**: Stored but not used (3D mode not implemented)

---

### Display Control

#### `void VISetBlack(BOOL black)`

**Purpose**: Enable/disable black screen

**GameCube/Wii**:
- VI hardware blanks video output
- Used during loading, transitions

**PC Implementation**:
- Sets flag (game can query and render black)
- Doesn't actually blank window

**Usage**:
```c
VISetBlack(TRUE);   // Black screen
LoadLevel();
VISetBlack(FALSE);  // Show game
```

---

#### `void VISet3D(BOOL threeD)`

**Purpose**: Enable 3D stereoscopic mode

**GameCube/Wii**: Configures VI for 3D display (requires special hardware)

**PC Implementation**: Stores flag (not implemented)

---

#### `void VIConfigure(const GXRenderModeObj* rm)`

**Purpose**: Configure video mode from GX render mode object

**GameCube/Wii**:
- Programs VI registers based on render mode
- Sets resolution, aspect ratio, interlace, filters

**PC Implementation**:
- No-op (render mode handled by GX)
- VI just provides window/context

---

#### `void VIConfigurePan(u16 xOrg, u16 yOrg, u16 width, u16 height)`

**Purpose**: Configure screen panning/cropping

**GameCube/Wii**: Adjusts visible area within frame buffer

**PC Implementation**: No-op (not applicable to modern displays)

---

### Query Functions

#### `u32 VIGetRetraceCount(void)`

**Purpose**: Get number of VBlanks since initialization

**Returns**: Increments every frame (60 per second @ 60Hz)

**Usage**:
```c
u32 startFrame = VIGetRetraceCount();
DoWork();
u32 endFrame = VIGetRetraceCount();
u32 framesElapsed = endFrame - startFrame;
```

---

#### `u32 VIGetNextField(void)`

**Purpose**: Get next field for interlaced modes

**Returns**: 
- `VI_FIELD_ABOVE` (even frames)
- `VI_FIELD_BELOW` (odd frames)

**GameCube/Wii**: Used for proper interlaced rendering

**PC Implementation**: Alternates based on retrace count

---

#### `u32 VIGetCurrentLine(void)`

**Purpose**: Get current scan line being drawn

**Returns**: Line number (0-480 for NTSC)

**GameCube/Wii**: Reads VI hardware register

**PC Implementation**: Returns 0 (not tracked)

---

#### `u32 VIGetTvFormat(void)`

**Purpose**: Get TV format

**Returns**:
- `VI_NTSC` (60Hz)
- `VI_PAL` (50Hz)
- `VI_MPAL` (Brazil)
- `VI_EURGB60` (European 60Hz)

---

#### `u32 VIGetScanMode(void)`

**Purpose**: Get scan mode

**Returns**:
- `VI_INTERLACE` (480i/576i)
- `VI_NON_INTERLACE` (240p/288p)
- `VI_PROGRESSIVE` (480p/576p)

---

#### `u32 VIGetDTVStatus(void)`

**Purpose**: Get digital TV status

**GameCube/Wii**: Component cable detection

**PC Implementation**: Returns 0 (no DTV hardware)

---

### Callbacks

#### `VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback callback)`

**Purpose**: Register callback called BEFORE each VBlank

**Callback Signature**: `void callback(u32 retraceCount)`

**Timing**: Called before frame buffer switch

**Usage**:
```c
void MyPreRetrace(u32 count) {
    // Called every frame before FB swap
    UpdateGameLogic();
    StartRendering();
}

VISetPreRetraceCallback(MyPreRetrace);
```

**Returns**: Previous callback (NULL if none)

---

#### `VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback callback)`

**Purpose**: Register callback called AFTER each VBlank

**Callback Signature**: `void callback(u32 retraceCount)`

**Timing**: Called after frame buffer switch

**Usage**:
```c
void MyPostRetrace(u32 count) {
    // Called every frame after FB swap
    SubmitAudioBuffers();
    ProcessInput();
}

VISetPostRetraceCallback(MyPostRetrace);
```

---

### Internal/Power Management Functions

These are stubs on PC:

- `void __VIResetSIIdle(void)` - Reset serial input idle timer
- `void __VIDisableDimming(void)` - Disable screen dimming
- `u32 __VISetDimmingCountLimit(u32)` - Set dimming timeout
- `BOOL __VIResetRFIdle(void)` - Reset RF idle timer
- `BOOL __VIResetDev0Idle(void)` through `__VIResetDev9Idle(void)` - Reset device idle timers

---

## PC-Specific Extensions

These functions are **NOT** in the original GC/Wii API:

### `SDL_Window* VIGetSDLWindow(void)`

**Purpose**: Get SDL window for rendering setup

**Usage**:
```c
SDL_Window* window = VIGetSDLWindow();
// GX module uses this to set up OpenGL
```

---

### `SDL_GLContext VIGetGLContext(void)`

**Purpose**: Get OpenGL context

**Usage**:
```c
SDL_GLContext ctx = VIGetGLContext();
SDL_GL_MakeCurrent(VIGetSDLWindow(), ctx);
```

---

### `void VIGetWindowSize(int* width, int* height)`

**Purpose**: Get current window dimensions

**Usage**:
```c
int w, h;
VIGetWindowSize(&w, &h);
glViewport(0, 0, w, h);
```

---

## Implementation Details

### Retrace Thread

Background thread simulates 60Hz VBlank timing:

```c
while (running) {
    Sleep(16ms);                    // Wait one frame
    
    if (preCallback)
        preCallback(retraceCount);
    
    currentFB = nextFB;             // Swap buffers
    
    retraceCount++;
    
    if (postCallback)
        postCallback(retraceCount);
}
```

### VSync Behavior

- SDL_GL_SetSwapInterval(1) locks to display refresh
- VIFlush() blocks until VBlank
- Prevents screen tearing
- Matches original 60Hz timing

---

## Typical Game Usage

```c
// Initialization
OSInit();
VIInit();
GXInit();

// Set up frame buffers
void* fb0 = GXAllocFrameBuffer();
void* fb1 = GXAllocFrameBuffer();

// Register callbacks
VISetPreRetraceCallback(UpdateLogic);
VISetPostRetraceCallback(ProcessAudio);

// Game loop
int currentFB = 0;
while (running) {
    // Set buffer to render to
    GXSetFrameBuffer(currentFB ? fb1 : fb0);
    
    // Render frame
    GXBegin();
    DrawScene();
    GXEnd();
    
    // Tell VI which buffer to display
    VISetNextFrameBuffer(currentFB ? fb1 : fb0);
    
    // Wait for VBlank
    VIWaitForRetrace();
    
    // Swap buffers
    VIFlush();
    
    // Toggle buffer
    currentFB ^= 1;
}
```

---

## Function Summary

| Function | Purpose | GC/Wii | PC |
|----------|---------|--------|-----|
| **VIInit** | Initialize video | Set up VI hardware | Create SDL window + OpenGL |
| **VIWaitForRetrace** | Wait for VBlank | Block on interrupt | Poll retrace counter |
| **VIFlush** | Flush config | Write VI registers | **Swap GL buffers** |
| **VISetNextFrameBuffer** | Set next FB | VI scans this buffer | Stored (informational) |
| **VIGetRetraceCount** | Frame counter | VBlank interrupt count | Retrace thread count |
| **VISetBlack** | Black screen | Hardware blanking | Flag only |
| **VIConfigure** | Video mode | Program VI registers | No-op |
| **VISetPreRetraceCallback** | Pre-VBlank callback | Interrupt handler | Thread callback |
| **VISetPostRetraceCallback** | Post-VBlank callback | Interrupt handler | Thread callback |

---

## Key Differences: GC/Wii vs PC

### Frame Buffer
- **GC/Wii**: XFB in main RAM, VI scans and outputs
- **PC**: OpenGL manages buffers, frame buffer pointers are symbolic

### VBlank
- **GC/Wii**: Hardware interrupt every 16.67ms
- **PC**: Software thread sleeps for 16ms

### Buffer Swap
- **GC/Wii**: VI hardware switches on VBlank (automatic)
- **PC**: VIFlush() calls SDL_GL_SwapWindow() (manual)

### Display Output
- **GC/Wii**: Analog/digital video to TV
- **PC**: Digital to monitor via OS graphics driver

---

## Performance Considerations

1. **VIFlush() is critical on PC** - This actually displays the frame
2. **VSync locks to display** - 60Hz on most monitors, 144Hz possible
3. **Callbacks run on separate thread** - Be thread-safe
4. **Window can resize** - Handle resize events in VIFlush()

---

## Module Statistics

- **Functions**: 26 public + 3 PC extensions
- **Source File**: `src/vi/VI.c` (658 lines)
- **Header File**: `include/dolphin/vi.h` (300 lines)
- **Total Lines**: ~960 lines

---

## Dependencies

- **SDL2**: Window creation, OpenGL context, event handling
- **OpenGL 3.3**: Graphics context (GX will render to this)
- **OS Module**: OSReport, OSSleepTicks, OSMillisecondsToTicks
- **Platform**: Windows (CreateThread) or POSIX (pthread)

---

## Next Steps

The VI module provides the display surface. The **GX module** will use VI's window and OpenGL context to implement the GameCube graphics API.

