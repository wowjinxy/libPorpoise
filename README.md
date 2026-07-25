# libPorpoise

A drop-in replacement for the GameCube/Wii SDK and a compatibility layer for the GameCube/Wii APIs on other platforms.

## Overview

libPorpoise provides a compatibility layer that allows GameCube and Wii games to be ported to other platforms with minimal code changes. By using a community-decompiled version of the SDK functions, the API stays the same and libPorpoise supports building for real GC/Wii hardware.

## Features

- **API Compatible**: Drop-in replacement for GC/Wii SDK functions
- **Cross-Platform**: Builds on GameCube, Windows, Linux. Build and debug a PC port and GameCube ELF file from the same source tree

## Project Structure

```
libPorpoise/
├── include/          # Public header files (GC/Wii SDK API)
│   └── dolphin/      # Dolphin SDK headers
├── src/              # Implementation files
│   ├── os/           # Original DolphinOS with conditional compilation to support other platforms
│   ├── gx/           # Graphics subsystem
│   ├── pad/          # Controller input
│   ├── card/         # Memory card
│   ├── dvd/          # Disc reading (wraps file system APIs on other platforms)
│   └── sim/          # Simulator library. Calls application main function, handles input, 
│                     # GX compatibility layer. Most code to make libPorpoise work on platforms 
│                     # other than GC.
└── standalone/       # Example program, used to test linking in CI
```

## Usage

Add to your meson project as a subproject:

subprojects/libPorpoise.wrap:
```
[wrap-git]
url = https://github.com/wowjinxy/libPorpoise.git
revision = master
depth = 1
directory = libPorpoise

[provide]
dependency_names = libPorpoise
```

Add the library in your meson.build:
```
libporpoise_dep = dependency('libPorpoise')
```

Add the following to your project's meson.options:
```
option('build_target', type: 'combo', choices: ['gc', 'win64', 'linux'], value: 'gc', description: 'Target build platform')
```


## Contributing

Contributions are welcome! Please ensure your code:
- Maintains API compatibility with the original SDK
- Includes appropriate documentation
- Passes all tests
- Follows the project coding style

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

This project reimplements the API of Nintendo's GameCube and Wii SDKs for preservation and porting purposes. SDK source came from the [pikmin decomp](https://github.com/projectPiki/pikmin).

