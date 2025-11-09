# PAD Configuration System

The PAD module includes a flexible configuration system that allows users to customize controller behavior without modifying code.

## Configuration File

Settings are stored in `pad_config.ini` in INI format. The file is automatically loaded when `PADInit()` is called.

## File Location

Place `pad_config.ini` in the same directory as your executable:

```
your_game/
├── your_game.exe
└── pad_config.ini
```

## Configuration Sections

### [Keyboard]

Rebind keyboard keys for Player 1 controller fallback. Uses SDL scancode names.

**Available Bindings:**
- `up`, `down`, `left`, `right` - D-pad / Main stick
- `a`, `b`, `x`, `y` - Face buttons
- `start` - START button
- `l`, `r`, `z` - Triggers
- `c_up`, `c_down`, `c_left`, `c_right` - C-stick

**SDL Key Names:**
See https://wiki.libsdl.org/SDL2/SDL_Scancode for full list.

Common keys: `A`-`Z`, `0`-`9`, `Space`, `Return`, `Escape`, `Tab`, `LeftShift`, `RightShift`, `LeftCtrl`, `RightCtrl`, `Up`, `Down`, `Left`, `Right`, `F1`-`F12`

**Example:**
```ini
[Keyboard]
; WASD movement
up=W
down=S
left=A
right=D

; Space to jump
a=Space
b=LeftShift
```

### [Gamepad]

Remap gamepad buttons (optional - defaults work for most controllers).

**Available Mappings:**
- `a`, `b`, `x`, `y` - Face buttons
- `start` - START button  
- `l`, `r` - Shoulder buttons
- `z` - Z trigger (usually right stick press)

**SDL Button Names:**
`a`, `b`, `x`, `y`, `start`, `back`, `guide`, `leftshoulder`, `rightshoulder`, `leftstick`, `rightstick`

**Example:**
```ini
[Gamepad]
; Swap A and B (like Nintendo vs Xbox layout)
a=b
b=a
```

### [Settings]

Adjust analog input behavior and rumble.

#### Dead Zones

Minimum distance from center before input registers.

- `stick_deadzone` - Main analog stick (0-127, default: 15)
- `c_stick_deadzone` - C-stick (0-127, default: 15)
- `trigger_deadzone` - L/R triggers (0-255, default: 30)

**Lower values** = more sensitive (input registers sooner)  
**Higher values** = less sensitive (larger dead zone)

**Example:**
```ini
; Tight dead zone for precise control
stick_deadzone=8

; Larger dead zone for worn controller
stick_deadzone=20
```

#### Sensitivity

Multiplier for analog stick input.

- `stick_sensitivity` - Main stick (0.1-2.0, default: 1.0)
- `c_stick_sensitivity` - C-stick (0.1-2.0, default: 1.0)

**< 1.0** = slower/less sensitive  
**> 1.0** = faster/more sensitive

**Example:**
```ini
; Faster camera movement
c_stick_sensitivity=1.5

; Slower character movement
stick_sensitivity=0.8
```

#### Rumble

- `rumble_intensity` - Rumble strength (0.0-1.0, default: 0.5)

**0.0** = off  
**1.0** = maximum

**Example:**
```ini
; Strong rumble
rumble_intensity=1.0

; Subtle rumble
rumble_intensity=0.3

; Disable rumble
rumble_intensity=0.0
```

## Complete Example

```ini
; libPorpoise PAD Configuration

[Keyboard]
; WASD + Space/Shift layout
up=W
down=S
left=A
right=D
a=Space
b=LeftShift
x=E
y=Q
start=Return
l=1
r=2
z=LeftCtrl
c_up=I
c_down=K
c_left=J
c_right=L

[Gamepad]
; Default mappings work for most controllers
; Uncomment to customize:
; a=b
; b=a

[Settings]
; Tight control for speedrunning
stick_deadzone=8
c_stick_deadzone=10
trigger_deadzone=20
stick_sensitivity=1.2
c_stick_sensitivity=1.5
rumble_intensity=0.7
```

## Runtime Behavior

- Config is loaded once at `PADInit()`
- Changes require restarting the application
- Missing config file uses built-in defaults
- Invalid values are ignored (defaults used)
- Syntax errors in file are logged but don't crash

## Programmatic Access

The configuration system is internal to the PAD module. Games don't need to interact with it directly - just place `pad_config.ini` in the executable directory.

## Best Practices

1. **Ship a default config** - Include `pad_config.ini` with sensible defaults
2. **Document controls** - Tell users which keys do what
3. **Test defaults** - Make sure default config works well
4. **Support no config** - Game should work without config file
5. **Provide examples** - Show users how to customize

## Troubleshooting

**Config not loading:**
- Check file is named exactly `pad_config.ini`
- Check file is in same directory as executable
- Check file encoding is UTF-8 or ASCII
- Look for error messages in console output

**Keys not working:**
- Verify SDL scancode names are correct
- Check for typos (case-sensitive)
- Test with default config first

**Settings not applying:**
- Check value ranges (dead zones 0-127, sensitivity 0.1-2.0, etc.)
- Verify INI syntax (key=value, no extra spaces in key names)
- Restart application after config changes

## See Also

- [SDL2_SETUP.md](SDL2_SETUP.md) - SDL2 installation and setup
- [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md) - Module status
- SDL Scancode reference: https://wiki.libsdl.org/SDL2/SDL_Scancode

