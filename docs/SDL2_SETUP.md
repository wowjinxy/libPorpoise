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

## Configuration File

libPorpoise supports customizing controller settings via `pad_config.ini`:

### Creating Config File

Copy the included sample file to the executable directory:
```bash
cp pad_config.ini build/bin/
```

### Customization Options

Edit `pad_config.ini` to customize:

**[Keyboard] section:**
- Rebind all keyboard keys (use SDL key names)
- Examples: `a=Space`, `up=W`, `start=Escape`

**[Gamepad] section:**
- Remap gamepad buttons (swap A/B, etc.)
- Uses SDL button names

**[Settings] section:**
- `stick_deadzone` - Dead zone radius (0-127, default: 15)
- `c_stick_deadzone` - C-stick dead zone (0-127, default: 15)
- `trigger_deadzone` - Trigger threshold (0-255, default: 30)
- `stick_sensitivity` - Analog sensitivity (0.1-2.0, default: 1.0)
- `c_stick_sensitivity` - C-stick sensitivity (0.1-2.0, default: 1.0)
- `rumble_intensity` - Rumble strength (0.0-1.0, default: 0.5)

### Example Custom Config

```ini
[Keyboard]
; WASD movement instead of arrows
up=W
down=S
left=A
right=D

; Different button layout
a=Space
b=LeftShift
x=E
y=Q

[Settings]
; More sensitive stick, less dead zone
stick_deadzone=10
stick_sensitivity=1.5

; Stronger rumble
rumble_intensity=0.8
```

The config is automatically loaded at `PADInit()`. Changes require restarting the application.

## Testing Controllers

Run the PAD example to test your controllers:
```bash
./build/bin/pad_test
```

This will display button presses, analog stick positions, and trigger values for all connected controllers.

