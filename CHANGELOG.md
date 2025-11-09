# Changelog

All notable changes to Porpoise SDK will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.1.0] - 2025-11-09

### 🎉 Initial Release - OS Module Complete!

**Major Achievement:** Complete implementation of the entire OS module from GameCube/Wii SDK.

### Added

#### Core OS Functions
- **OS.c** (214 lines)
  - `OSInit()` - System initialization with memory arena setup
  - `OSReport()` - Debug output with timestamps
  - `OSPanic()` - Fatal error handler
  - Memory arena management (MEM1 24MB + MEM2 64MB simulation)
  - Console type detection

#### Threading System  
- **OSThread.c** (600 lines)
  - Complete threading implementation using Win32/POSIX
  - `OSCreateThread()`, `OSResumeThread()`, `OSSuspendThread()`
  - Priority mapping (32 levels → OS priorities)
  - Thread-local storage (2 slots)
  - `OSYieldThread()`, `OSSleepTicks()`, `OSJoinThread()`
  
- **OSMutex** (in OSThread.c, 100 lines)
  - Recursive mutex support
  - `OSInitMutex()`, `OSLockMutex()`, `OSUnlockMutex()`, `OSTryLockMutex()`
  
- **OSCond** (in OSThread.c)
  - Condition variables
  - `OSInitCond()`, `OSWaitCond()`, `OSSignalCond()`

#### Synchronization Primitives
- **OSSemaphore.c** (425 lines)
  - Counting semaphores
  - `OSInitSemaphore()`, `OSWaitSemaphore()`, `OSSignalSemaphore()`
  - `OSTryWaitSemaphore()`, `OSGetSemaphoreCount()`
  
- **OSMessage.c** (450 lines)
  - Thread-safe message queues
  - `OSInitMessageQueue()`, `OSSendMessage()`, `OSReceiveMessage()`
  - `OSJamMessage()` for priority messages

#### Memory Management
- **OSAlloc.c** (797 lines)
  - Full custom heap allocator
  - First-fit allocation with coalescing
  - `OSCreateHeap()`, `OSAllocFromHeap()`, `OSFreeToHeap()`
  - Multiple heap support
  - `OSAllocFixed()` for fixed addresses
  - `OSCheckHeap()`, `OSDumpHeap()` debugging

#### Timing System
- **OSTime.c** (700 lines)
  - High-resolution time base (40.5 MHz simulation)
  - `OSGetTime()`, `OSGetTick()`, `OSGetSystemTime()`
  - Full calendar conversion with leap year support
  - `OSTicksToCalendarTime()`, `OSCalendarTimeToTicks()`
  
- **OSAlarm.c** (545 lines)
  - Hardware-accurate alarm system
  - Background timer thread
  - Sorted alarm queue
  - `OSSetAlarm()`, `OSSetPeriodicAlarm()`, `OSCancelAlarm()`

#### Cache Operations
- **OSCache.c** (400 lines)
  - Dual-mode implementation:
    * **Simple Mode** (default): No-op stubs, zero overhead
    * **Full Mode** (`PORPOISE_USE_GECKO_MEMORY`): Complete emulation
  - Locked cache support (16KB scratchpad)
  - `LCEnable()`, `LCLoadData()`, `LCStoreData()`, `LCAlloc()`
  
- **GeckoMemory.c** (201 lines)
  - Full Gekko/Broadway memory layout
  - 24MB MEM1 + 64MB MEM2 + 16KB locked cache
  - Virtual address translation
  - Big-endian byte order handling

#### Error Handling
- **OSError.c** (400 lines)
  - Error handler registration for 17 error types
  - `OSSetErrorHandler()`, `__OSGetErrorHandler()`
  - `__OSUnhandledException()` with detailed crash reporting
  - Exception context dumping

- **OSContext.c** (300 lines)
  - Context management functions
  - `OSClearContext()`, `OSInitContext()`, `OSDumpContext()`
  - Documented limitations on PC

#### System Functions
- **OSReset.c** (625 lines)
  - Reset and shutdown handling
  - Priority-based shutdown function queue
  - `OSRebootSystem()`, `OSShutdownSystem()`, `OSRestart()`
  - `OSReturnToMenu()`, `OSReturnToDataManager()`
  - Save region management
  
- **OSResetSW.c** (400 lines)
  - Reset button handling
  - `OSGetResetButtonState()`, `OSSetResetCallback()`
  - `OSSimulateResetButton()` (PC extension)
  - Debouncing implementation

#### Configuration & RTC
- **OSRtc.c** (500 lines)
  - Real-time clock using OS time APIs
  - SRAM configuration file (`porpoise_sram.cfg`)
  - `OSGetSoundMode()`, `OSSetSoundMode()`
  - `OSGetVideoMode()`, `OSSetVideoMode()`
  - `OSGetLanguage()`, `OSSetLanguage()`
  - `OSGetProgressiveMode()`, `OSSetProgressiveMode()`

#### Hardware Interfaces
- **OSInterrupt.c** (532 lines)
  - Interrupt handler registration (28 interrupt types)
  - `__OSSetInterruptHandler()`, `__OSGetInterruptHandler()`
  - Interrupt masking APIs
  - Migration strategies documented

- **OSMemory.c** (435 lines)
  - Memory sizing functions
  - `OSGetPhysicalMem1Size()`, `OSGetPhysicalMem2Size()`
  - `OSProtectRange()` (documented stub)

#### Font & Text
- **OSFont.c** (500 lines)
  - Complete UTF conversion utilities
  - `OSUTF8to32()`, `OSUTF32to8()`, `OSUTF16to32()`, `OSUTF32to16()`
  - `OSANSItoUTF32()`, `OSUTF32toANSI()`
  - Font rendering functions (documented stubs)

### Documentation

#### Comprehensive Guides
- **MEMORY_EMULATION.md** - Dual-mode memory system guide
- **THREADING_ARCHITECTURE.md** - Cooperative vs preemptive threading
- **FUTURE_EXCEPTION_HANDLING.md** - Exception handler roadmap
- **IMPLEMENTATION_STATUS.md** - Detailed status tracking
- **README.md** - Project overview
- **TODO.md** - Development roadmap

### Examples

#### Working Demos
- **heap_example.c** - Memory allocation patterns
- **alarm_example.c** - Timer and callback usage
- **thread_test.c** - Multi-threading demo
- **locked_cache_example.c** - Full memory emulation demo
- **reset_button_example.c** - Reset handling integration

### Build System

#### CMake Configuration
- Cross-platform build system (CMake 3.10+)
- Static library generation (default)
- Shared library option (`PORPOISE_BUILD_SHARED`)
- Full memory emulation option (`PORPOISE_USE_GECKO_MEMORY`)
- Install target
- Example compilation

#### Platform Support
- Windows (MSVC, MinGW)
- Linux (GCC, Clang)
- macOS (Clang)

---

## [Unreleased]

### Planned for v0.2.0
- GX (Graphics) module
- PAD (Controller) module
- VI (Video) module
- Basic rendering demo

### Planned for v0.3.0
- AX (Audio) module
- DVD (File I/O) module
- CARD (Save data) module
- Complete game demo

---

## Version History

### v0.1.0 - 2025-11-09
- 🎉 **Initial release**
- ✅ **Complete OS module** (16 files, 13,500+ lines)
- ✅ **5 working examples**
- ✅ **6 comprehensive documentation guides**
- ✅ **Cross-platform support** (Win/Linux/Mac)

---

**Project Started:** 2025-11-09  
**OS Module Completed:** 2025-11-09  
**Total Development Time:** 1 day 🚀

---

[Unreleased]: https://github.com/yourusername/Porpoise_SDK/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/yourusername/Porpoise_SDK/releases/tag/v0.1.0
