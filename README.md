# libPorpoise

A drop-in replacement for the GameCube/Wii SDK designed for PC ports.

## Overview

libPorpoise provides a compatibility layer that allows GameCube and Wii games to be ported to PC with minimal code changes. By replicating the original SDK's API surface, developers can port their games while maintaining the same programming model.

## Features

- **API Compatible**: Drop-in replacement for GC/Wii SDK functions
- **Cross-Platform**: Builds on Windows, Linux, and macOS
- **Modern Backend**: Uses modern graphics APIs and input systems while maintaining the classic interface
- **Modular Design**: Include only the modules you need
- **Dual Mode**: Simple mode for basic ports, or full memory emulation for advanced features

## Project Structure

```
libPorpoise/
├── include/          # Public header files (GC/Wii SDK API)
│   ├── dolphin/      # Dolphin SDK headers
│   └── revolution/   # Revolution SDK headers
├── src/              # Implementation files
│   ├── os/           # Operating system functions
│   ├── gx/           # Graphics subsystem
│   ├── pad/          # Controller input
│   ├── card/         # Memory card
│   └── dvd/          # Disc reading
├── lib/              # Built libraries
└── examples/         # Example programs
```

## Quick Start

See [QUICKSTART.md](QUICKSTART.md) for detailed build instructions and getting started guide.

**TL;DR:**
```bash
# Linux/macOS - Simple mode
./build.sh

# Windows - Simple mode
build.bat

# Enable full memory emulation (for locked cache support)
cmake .. -DPORPOISE_USE_GECKO_MEMORY=ON
cmake --build .
```

See [MEMORY_EMULATION.md](docs/MEMORY_EMULATION.md) for details on memory emulation modes.

## Architecture Notes

libPorpoise adapts the original cooperative threading model to modern preemptive OS threads. See [THREADING_ARCHITECTURE.md](docs/THREADING_ARCHITECTURE.md) for detailed explanation of differences and migration patterns.

## Usage

Link against the libPorpoise library and include the appropriate headers:

```c
#include <dolphin/os.h>
#include <dolphin/gx.h>
#include <dolphin/pad.h>

int main() {
    OSInit();
    // Your game code here
    return 0;
}
```

## Module Status

| Module | Status | Description |
|--------|--------|-------------|
| **OS** | ✅ **Complete** | Operating system and threading (16 modules, 13,500+ lines) |
| **PAD** | ✅ **Complete** | Controller input (SDL2 + keyboard fallback) |
| GX     | 📋 Planned | Graphics subsystem |
| CARD   | 📋 Planned | Memory card operations |
| DVD    | 📋 Planned | Disc I/O |
| AX/DSP | 📋 Planned | Audio subsystem |

**v0.1.0 - OS Module Complete!** See [IMPLEMENTATION_STATUS.md](docs/IMPLEMENTATION_STATUS.md) for detailed breakdown.

## Contributing

Contributions are welcome! Please ensure your code:
- Maintains API compatibility with the original SDK
- Includes appropriate documentation
- Passes all tests
- Follows the project coding style

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

This project reimplements the API of Nintendo's GameCube and Wii SDKs for preservation and porting purposes.

