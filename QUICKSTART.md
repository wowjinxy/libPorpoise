# libPorpoise Quick Start Guide

Get up and running with libPorpoise in minutes!

## Prerequisites

### Windows
- Visual Studio 2019 or later (with C++ support)
- Or MinGW-w64
- CMake 3.15 or later

### Linux
- GCC or Clang
- CMake 3.15 or later
- Build essentials: `sudo apt install build-essential cmake`

### macOS
- Xcode Command Line Tools: `xcode-select --install`
- CMake: `brew install cmake`

## Building the SDK

### Option 1: Using Build Scripts (Recommended)

**Windows:**
```cmd
build.bat
```

**Linux/macOS:**
```bash
chmod +x build.sh
./build.sh
```

### Option 2: Manual Build

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Running Examples

After building, run the example programs:

**Windows:**
```cmd
.\build\bin\Release\simple_example.exe
.\build\bin\Release\thread_example.exe
```

**Linux/macOS:**
```bash
./build/bin/simple_example
./build/bin/thread_example
```

## Using libPorpoise in Your Project

### 1. Install the SDK

```bash
cd build
cmake --install . --prefix /path/to/install
```

### 2. Link Against libPorpoise

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.15)
project(MyGame)

# Find libPorpoise
include_directories(/path/to/install/include)
link_directories(/path/to/install/lib)

add_executable(mygame main.c)
target_link_libraries(mygame porpoise)
```

### 3. Write Your Code

**main.c:**
```c
#include <dolphin/os.h>

int main(void) {
    OSInit();
    OSReport("Hello from libPorpoise!\n");
    return 0;
}
```

### 4. Build Your Project

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Next Steps

- Check out the [examples](examples/) directory for more complex usage
- Read the [API documentation](docs/) (coming soon)
- See [CONTRIBUTING.md](CONTRIBUTING.md) to help develop the SDK

## Troubleshooting

### Build Fails on Windows
- Make sure you have Visual Studio C++ tools installed
- Try running from "Developer Command Prompt for VS"

### Build Fails on Linux
- Install required packages: `sudo apt install build-essential cmake`
- Ensure you have pthread support

### Examples Don't Run
- Make sure you built in Release mode or adjust the path
- On Linux, you may need to set `LD_LIBRARY_PATH`

## Getting Help

- Open an issue on GitHub
- Check existing issues for similar problems
- Review the documentation and examples

