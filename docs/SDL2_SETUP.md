# SDL2 Setup for libPorpoise

The PAD (controller) module requires SDL2 for gamepad input support.

## Windows

### Option 1: vcpkg (Recommended)
```cmd
vcpkg install sdl2:x64-windows
```

Then configure CMake with:
```cmd
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]\scripts\buildsystems\vcpkg.cmake
```

### Option 2: Manual Installation
1. Download SDL2 development libraries from https://www.libsdl.org/download-2.0.php
2. Extract to a location (e.g., `C:\SDL2`)
3. Set environment variable: `SDL2_DIR=C:\SDL2`
4. Configure CMake:
```cmd
cmake .. -DSDL2_DIR=C:\SDL2
```

## Linux

### Ubuntu/Debian
```bash
sudo apt-get install libsdl2-dev
```

### Fedora
```bash
sudo dnf install SDL2-devel
```

### Arch
```bash
sudo pacman -S sdl2
```

## macOS

### Homebrew
```bash
brew install sdl2
```

### MacPorts
```bash
sudo port install libsdl2
```

## Verifying Installation

After installing SDL2, reconfigure and build:
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

You should see SDL2 found in the CMake output:
```
-- Found SDL2: /path/to/SDL2
```

## Keyboard Fallback

If SDL2 is not available or gamepad initialization fails, the PAD module will automatically fall back to keyboard input for Player 1:

- **Arrow keys** - D-pad / Main stick
- **Z, X, C, V** - A, B, X, Y buttons
- **A, S, D** - L, R, Z triggers
- **I, K, J, L** - C-stick
- **Enter** - START button

## Testing Controllers

Run the PAD example to test your controllers:
```bash
./build/bin/pad_test
```

This will display button presses, analog stick positions, and trigger values for all connected controllers.

