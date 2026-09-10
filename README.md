# libPorpoise

A drop-in replacement for the GameCube/Wii SDK and a compatibility layer for the GameCube/Wii APIs on other platforms.

## Overview

libPorpoise provides a compatibility layer that allows GameCube and Wii games to be ported to other platforms with minimal code changes. By using a community-decompiled version of the SDK functions, the API stays the same and libPorpoise supports building for real GC/Wii hardware.

## Features

- **API Compatible**: Drop-in replacement for GC/Wii SDK functions
- **Cross-Platform**: Builds on GameCube, Windows, Linux. Build and debug a PC port and GameCube ELF file from the same source tree

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

For the most part, source code changes to decomps will not be necessary. However, there are a few exceptions:
* Big endian byte order will still need to be fixed when porting to little endian platforms
* * GX vertex data is expected to be stored in the native endian of your target platform
* * Display lists created through GXBeginDisplayList/GXEndDisplayList will be in native endian and flagged as such by the GX compatibility layer
* * All other GX display lists will be treated as big endian display lists and are supported in big endian format without conversion
* Some include paths may need to be adjusted

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments/Credits

Huge thanks to [wowjinxy](https://github.com/wowjinxy/) for originally starting this project and providing ongoing support.

Check out [PorpoiseTool](github.com/wowjinxy/Porpoise-Tool) for a static recompilation tool based on libPorpoise

SDK source oritingated from the [pikmin decomp](https://github.com/projectPiki/pikmin).

