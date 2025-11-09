# Contributing to Porpoise SDK

Thank you for your interest in contributing to Porpoise SDK! This document provides guidelines and information for contributors.

## Code of Conduct

- Be respectful and inclusive
- Focus on constructive feedback
- Help maintain a welcoming environment

## How to Contribute

### Reporting Bugs

When reporting bugs, please include:
- Your operating system and version
- Steps to reproduce the issue
- Expected vs actual behavior
- Any relevant error messages or logs

### Suggesting Features

Feature suggestions should include:
- Clear description of the feature
- Use cases and benefits
- How it relates to the original GC/Wii SDK

### Submitting Code

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Test your changes
5. Commit with clear messages (`git commit -m 'Add amazing feature'`)
6. Push to your branch (`git push origin feature/amazing-feature`)
7. Open a Pull Request

## Development Guidelines

### API Compatibility

The primary goal is API compatibility with the original GameCube/Wii SDK:
- Match function signatures exactly
- Use the same type names and conventions
- Preserve behavior as documented in the original SDK

### Code Style

- Use 4 spaces for indentation (no tabs)
- Opening braces on the same line for functions
- Clear, descriptive variable names
- Comment non-obvious implementations
- Keep line length reasonable (under 100 characters preferred)

Example:
```c
void OSExampleFunction(u32 param) {
    // Clear comment explaining non-obvious behavior
    if (param > MAX_VALUE) {
        return;
    }
    
    // Implementation here
}
```

### Platform Support

Code should work on:
- Windows (MSVC, MinGW)
- Linux (GCC, Clang)
- macOS (Clang)

Use preprocessor directives for platform-specific code:
```c
#ifdef _WIN32
    // Windows implementation
#else
    // POSIX implementation
#endif
```

### Testing

- Test on multiple platforms when possible
- Include example programs demonstrating new features
- Ensure existing examples continue to work

### Documentation

- Update README.md if adding major features
- Document public APIs in header files
- Include usage examples for complex features

## Module Implementation Priority

Current focus areas (in order):
1. OS (Operating System) - In Progress
2. GX (Graphics) - Planned
3. PAD (Controller Input) - Planned
4. CARD (Memory Card) - Planned
5. DVD (Disc I/O) - Planned

## Building and Testing

```bash
mkdir build
cd build
cmake ..
cmake --build .

# Run examples
./bin/simple_example
./bin/thread_example
```

## Questions?

Feel free to open an issue for:
- Questions about implementation
- Clarification on SDK behavior
- Discussion of design decisions

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

