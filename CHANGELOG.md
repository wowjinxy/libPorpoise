# Changelog

All notable changes to Porpoise SDK will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2025-11-09

### Added
- Initial project structure
- Basic OS module implementation
  - `OSInit()` and initialization system
  - Thread creation and management (OSCreateThread, OSResumeThread, etc.)
  - Mutex synchronization (OSInitMutex, OSLockMutex, OSUnlockMutex)
  - Time functions (OSGetTime, OSTicksToCalendarTime)
  - Alarm system (OSCreateAlarm, OSSetAlarm, OSSetPeriodicAlarm)
  - Utility functions (OSReport, OSGetConsoleType)
- Cross-platform support (Windows, Linux, macOS)
- CMake build system
- Example programs
  - simple.c - Basic initialization and time example
  - thread_test.c - Threading and mutex example
- Documentation
  - README.md - Project overview
  - QUICKSTART.md - Getting started guide
  - API.md - API reference
  - CONTRIBUTING.md - Contribution guidelines
- Build scripts (build.sh, build.bat)
- Git configuration (.gitignore, .gitattributes)
- MIT License

### Platform Support
- Windows (MSVC, MinGW)
- Linux (GCC, Clang)
- macOS (Clang)

## [Unreleased]

### Planned
- GX (Graphics) module
- PAD (Controller) module
- CARD (Memory Card) module
- DVD (Disc I/O) module
- AX/DSP (Audio) module
- Unit tests
- CI/CD pipeline
- Additional examples

