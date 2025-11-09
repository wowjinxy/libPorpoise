# Porpoise SDK Development TODO

This document tracks planned features, improvements, and known issues.

## High Priority

### OS Module Enhancements
- [x] Implement proper thread local storage (TLS)
- [ ] Add OSThread queue management (ready, suspended, waiting)
- [x] Implement condition variables
- [x] Add semaphore support
- [x] Implement message queues
- [ ] Add platform exception handler integration (SEH/signals)
  * Windows: SetUnhandledExceptionFilter() or __try/__except
  * Linux: sigaction() for SIGSEGV, SIGFPE, SIGILL, SIGBUS
  * Convert platform exceptions to OSError types
  * Convert platform context to OSContext
  * Call registered error handlers
  * See OSError.c header comment for detailed design notes

### Memory Management
- [x] Implement heap management (OSAllocFromHeap, OSFreeToHeap)
- [x] Add arena allocator
- [ ] Memory protection and bounds checking (OSProtectRange)
- [ ] Memory card simulation in RAM

### Build System
- [ ] Add unit testing framework
- [ ] Set up CI/CD (GitHub Actions)
- [ ] Create install targets
- [ ] Add pkg-config support
- [ ] Generate API documentation (Doxygen)

## Medium Priority

### GX Module (Graphics)
- [ ] Design GX API structure
- [ ] Implement immediate mode rendering
- [ ] OpenGL backend
- [ ] Vulkan backend option
- [ ] Texture management
- [ ] Transform pipeline
- [ ] Lighting system
- [ ] Fog effects

### PAD Module (Controller Input)
- [ ] Controller input abstraction
- [ ] SDL2 integration for controller support
- [ ] Keyboard/mouse fallback
- [ ] Rumble support
- [ ] Multiple controller support
- [ ] Button mapping configuration

### CARD Module (Memory Card)
- [ ] Memory card file system emulation
- [ ] Save data management
- [ ] Directory structure
- [ ] File read/write operations
- [ ] Async operations

### DVD Module (Disc I/O)
- [ ] Virtual file system
- [ ] File reading from directories
- [ ] Archive support (ISO, GCM)
- [ ] Async read operations
- [ ] Path mapping

## Low Priority

### AX/DSP Module (Audio)
- [ ] Audio mixer
- [ ] 3D positional audio
- [ ] Audio streaming
- [ ] Voice management
- [ ] Effect processing

### VI Module (Video Interface)
- [ ] Frame buffer management
- [ ] Resolution scaling
- [ ] VSync control
- [ ] Multiple display support

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

## Documentation
- [ ] Complete API reference for all modules
- [ ] Architecture documentation
- [ ] Porting guide (GC/Wii to PC)
- [ ] Tutorial series
- [ ] Video tutorials
- [ ] FAQ

## Testing
- [ ] Unit tests for OS module
- [ ] Integration tests
- [ ] Platform-specific test suite
- [ ] Performance benchmarks
- [ ] Memory leak tests
- [ ] Thread safety tests

## Examples
- [ ] Graphics rendering example
- [ ] Controller input example
- [ ] Audio playback example
- [ ] File I/O example
- [ ] Complete mini-game demo

## Known Issues
- [ ] Alarm system not yet dispatched (needs timer thread)
- [ ] Thread priorities not strictly enforced
- [ ] No current thread tracking across platform threads
- [ ] Mutex doesn't support recursive locking

## Nice to Have
- [ ] Plugin system for custom backends
- [ ] Shader compatibility layer
- [ ] Asset converter tools
- [ ] IDE integration (VS Code extension)
- [ ] Hot reload support for development

---

**Legend:**
- [ ] Not started
- [~] In progress
- [x] Completed

**Last Updated:** 2025-11-09

