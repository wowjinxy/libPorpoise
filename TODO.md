# libPorpoise Development TODO

This document tracks planned features, improvements, and known issues.

**Last Updated:** 2025-11-09

---

## ✅ COMPLETED: OS Module (v0.1.0)

**🎉 ALL 16 OS MODULES COMPLETE - 13,500+ LINES!**

### Core Systems ✅
- [x] OS initialization and arena management
- [x] Threading and synchronization (Win32/POSIX)
- [x] Memory allocation (custom heap with coalescing)
- [x] Timers and alarms (background thread)
- [x] Cache operations (dual-mode: simple + full emulation)
- [x] Context management (documented limitations)
- [x] Error handling and crash reporting
- [x] Font utilities (UTF conversion complete)
- [x] Interrupt callback system (documented migration)
- [x] Memory sizing and protection
- [x] Message queues (producer-consumer)
- [x] Reset and shutdown (callback queue)
- [x] Reset button handling (manual simulation)
- [x] Real-time clock and SRAM (file-based config)
- [x] Semaphores (counting)
- [x] Time base and calendar conversion

### Platform Support ✅
- [x] Windows (Win32 threads, critical sections)
- [x] Linux (POSIX threads, mutexes)
- [x] macOS (POSIX threads)

### Documentation ✅
- [x] MEMORY_EMULATION.md - Dual-mode guide
- [x] THREADING_ARCHITECTURE.md - Threading deep dive
- [x] FUTURE_EXCEPTION_HANDLING.md - Exception roadmap
- [x] IMPLEMENTATION_STATUS.md - Status tracking
- [x] Inline documentation (13,000+ lines of comments)

### Examples ✅
- [x] heap_example.c - Memory allocation
- [x] alarm_example.c - Timer callbacks
- [x] thread_test.c - Threading demo
- [x] locked_cache_example.c - Full memory mode
- [x] reset_button_example.c - Reset handling

---

## High Priority (v0.2.0)

### AR Module (ARAM - Audio RAM) ✅
- [x] ARInit - Initialize 16MB audio RAM simulation
- [x] ARAlloc/ARFree - ARAM allocation system
- [x] ARGetSize/ARGetBaseAddress - Query ARAM info
- [x] ARStartDMA - DMA transfers (instant memcpy on PC)
- [x] ARQInit - Queue system for large transfers
- [x] ARQPostRequest - Queued DMA operations

### VI Module (Video Interface) 🔄
- [ ] VIInit - Initialize video system
- [ ] VIFlush - Flush frame buffer
- [ ] VISetBlack - Black screen control
- [ ] VIWaitForRetrace - Wait for vblank
- [ ] Frame buffer management
- [ ] Resolution/video mode stubs

### GX Module (Graphics)
- [ ] Design GX API structure (TEV, transform pipeline)
- [ ] Implement immediate mode rendering
- [ ] OpenGL 3.3+ backend
- [ ] Vulkan backend option (future)
- [ ] Texture management (GXTexObj)
- [ ] Transform pipeline (matrices, positions, normals, colors)
- [ ] Lighting system (8 lights)
- [ ] Fog effects
- [ ] Display list support
- [ ] Geometry (quads, triangles, triangle strips)

### PAD Module (Controller Input) ✅
- [x] Controller input abstraction
- [x] SDL2 gamepad integration
- [x] Keyboard/mouse fallback
- [x] Rumble support (if available)
- [x] Multiple controller support (4 controllers)
- [x] Button mapping configuration (INI file)
- [x] Analog stick calibration
- [x] Configuration system (pad_config.ini)
  - [x] Keyboard rebinding
  - [x] Gamepad button remapping
  - [x] Adjustable dead zones
  - [x] Sensitivity multipliers
  - [x] Rumble intensity control

### VI Module (Video Interface)
- [ ] Frame buffer management
- [ ] Resolution scaling
- [ ] VSync control
- [ ] Multiple display support
- [ ] XFB (External Frame Buffer) emulation

---

## Medium Priority (v0.3.0)

### CARD Module (Memory Card)
- [ ] Memory card file system emulation
- [ ] Save data management
- [ ] Directory structure
- [ ] File read/write operations
- [ ] Async operations
- [ ] Multiple card support

### DVD Module (Disc I/O) ✅
- [x] Virtual file system (maps to "files/" directory)
- [x] File reading from directories
- [x] DVDOpen/DVDClose/DVDRead API
- [x] Async read operations (background threads)
- [x] Path mapping (configurable root directory)
- [x] Directory operations (DVDOpenDir/DVDReadDir/DVDChangeDir)
- [x] Synchronous and asynchronous reads
- [ ] Archive support (ISO, GCM) - Future enhancement

### AX/DSP Module (Audio)
- [ ] Audio mixer
- [ ] 3D positional audio
- [ ] Audio streaming
- [ ] Voice management
- [ ] Effect processing (reverb, delay, etc.)

---

## Low Priority (v0.4.0+)

### OS Module Enhancements
- [ ] Platform exception handler integration (SEH/signals)
  * Windows: SetUnhandledExceptionFilter()
  * Linux: sigaction() for SIGSEGV, SIGFPE, SIGILL, SIGBUS
  * See FUTURE_EXCEPTION_HANDLING.md for design
- [ ] Memory protection implementation (VirtualProtect/mprotect)
- [ ] OSThread queue management (ready, suspended, waiting)
  * Full cooperative scheduler emulation (if needed)
  * Probably not needed - OS scheduler works fine

### Network Module
- [ ] Socket emulation
- [ ] Network play support
- [ ] HTTP client

### Additional Features
- [ ] Debugger integration
- [ ] Performance profiling tools
- [ ] Memory leak detection
- [ ] Save state support
- [ ] Replay recording

---

## Build System

### Current ✅
- [x] CMake build system
- [x] Static library generation
- [x] Shared library option
- [x] Example programs
- [x] Platform detection (Win32/POSIX)

### TODO
- [ ] Add unit testing framework (Catch2 or Google Test)
- [ ] Set up CI/CD (GitHub Actions)
  * Build on Windows, Linux, macOS
  * Run tests automatically
  * Generate artifacts
- [ ] Create install targets
- [ ] Add pkg-config support (.pc file)
- [ ] Generate API documentation (Doxygen)
- [ ] Create release packages (ZIP/tar.gz)

---

## Documentation

### Current ✅
- [x] README.md - Project overview
- [x] Architecture documentation (threading, memory)
- [x] Implementation status tracking
- [x] Future enhancement roadmaps

### TODO
- [ ] Complete API reference for all modules (Doxygen)
- [ ] Porting guide (GC/Wii game to PC step-by-step)
- [ ] Tutorial series (beginner to advanced)
- [ ] Video tutorials (YouTube)
- [ ] FAQ (common questions)
- [ ] Performance optimization guide

---

## Testing

### TODO
- [ ] Unit tests for OS module functions
- [ ] Integration tests (multi-module)
- [ ] Platform-specific test suite
- [ ] Performance benchmarks
- [ ] Memory leak tests (Valgrind, ASan)
- [ ] Thread safety tests (TSan)
- [ ] Stress tests (thousands of threads, allocations)

---

## Known Issues

### Fixed ✅
- [x] ~~Alarm system not dispatched~~ - Background thread implemented
- [x] ~~Thread priorities not enforced~~ - Mapped to OS priorities
- [x] ~~Mutex doesn't support recursion~~ - Recursive locking implemented
- [x] ~~No current thread tracking~~ - Per-thread tracking added

### Remaining (By Design)
- Context save/restore not implemented - Not needed on PC (use setjmp/longjmp)
- Scheduler control not working - Can't disable OS scheduler (use mutexes)
- Hardware interrupts not triggered - No hardware (use polling/callbacks)
- Memory protection not enforced - OS provides protection (use debugger watchpoints)

These are **not bugs** - they're fundamental differences between bare-metal PowerPC and PC operating systems.

---

## Nice to Have (Future)

- [ ] Plugin system for custom backends
- [ ] Shader compatibility layer (GX → GLSL/HLSL)
- [ ] Asset converter tools (TPL → PNG, etc.)
- [ ] IDE integration (VS Code extension)
- [ ] Hot reload support for development
- [ ] Profiler visualization tool
- [ ] Memory map viewer
- [ ] Thread activity timeline

---

## Version Roadmap

### v0.1.0 (Current) ✅
- ✅ Complete OS module (16 files, 13,500+ lines)
- ✅ Dual memory emulation modes
- ✅ Cross-platform support (Win/Linux/Mac)
- ✅ Build system and examples
- ✅ Comprehensive documentation

### v0.2.0 (Next)
- [ ] GX (Graphics) module - Basic rendering
- [ ] PAD (Controller) module - Input handling
- [ ] VI (Video) module - Display output
- [ ] Simple rendering demo

### v0.3.0 (Future)
- [ ] AX (Audio) module
- [ ] DVD (File I/O) module
- [ ] CARD (Save data) module
- [ ] Complete game demo

### v0.4.0 (Future)
- [ ] Advanced features (network, debugger)
- [ ] Performance optimizations
- [ ] Additional platform support

---

**Legend:**
- [x] Completed
- [~] In progress
- [ ] Not started

**Next Steps:**
Focus on GX, PAD, and VI modules to enable basic game rendering and input.

---

