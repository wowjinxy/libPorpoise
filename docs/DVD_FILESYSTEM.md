# DVD File System

The DVD module provides disc file I/O compatible with GameCube/Wii SDK, mapped to a local directory structure on PC.

## Overview

On GameCube/Wii, games read assets from the disc drive. On PC, libPorpoise maps these operations to a local `files/` directory, making it easy to organize and access game assets.

## Directory Structure

All game files are placed in the `files/` directory:

```
your_game/
├── your_game.exe
├── pad_config.ini
└── files/                  ← DVD root directory
    ├── data/
    │   ├── stage1.dat
    │   ├── stage2.dat
    │   └── common.dat
    ├── models/
    │   ├── player.mdl
    │   └── enemy.mdl
    ├── textures/
    │   └── sprites.tpl
    └── audio/
        └── music.dsp
```

## Usage

### Initialize DVD System

```c
#include <dolphin/dvd.h>

DVDInit();  // Creates/verifies "files/" directory exists
```

### Open and Read Files

```c
DVDFileInfo file;
char buffer[1024];

// Open file (path relative to files/)
if (DVDOpen("data/stage1.dat", &file)) {
    // Read entire file
    s32 bytesRead = DVDRead(&file, buffer, file.length, 0);
    
    // Close when done
    DVDClose(&file);
}
```

### Partial Reads

```c
// Read 512 bytes starting at offset 1024
DVDRead(&file, buffer, 512, 1024);
```

### Asynchronous Reads

For loading large files without blocking:

```c
void LoadCallback(s32 result, DVDFileInfo* fileInfo) {
    if (result > 0) {
        OSReport("Loaded %d bytes\n", result);
    }
}

// Start async read
DVDFileInfo file;
char* bigBuffer = malloc(1024 * 1024);

if (DVDOpen("textures/bigfile.tpl", &file)) {
    DVDReadAsync(&file, bigBuffer, file.length, 0, LoadCallback);
    // Continue doing other work...
}
```

### Directory Operations

```c
// Change to subdirectory
DVDChangeDir("data");

// Now files open relative to files/data/
DVDOpen("stage1.dat", &file);  // Opens files/data/stage1.dat

// Get current directory
char path[256];
DVDGetCurrentDir(path, sizeof(path));
OSReport("Current dir: %s\n", path);  // Prints: /data

// Return to root
DVDChangeDir("/");
```

### Directory Listing

```c
DVDDir dir;
DVDDirEntry entry;

if (DVDOpenDir("models", &dir)) {
    while (DVDReadDir(&dir, &entry)) {
        if (entry.isDir) {
            OSReport("  [DIR]  %s\n", entry.name);
        } else {
            OSReport("  [FILE] %s\n", entry.name);
        }
    }
    DVDCloseDir(&dir);
}
```

## Path Resolution

Paths can be absolute or relative:

```c
// Absolute (from DVD root)
DVDOpen("/data/stage1.dat", &file);      // files/data/stage1.dat

// Relative (from current directory)
DVDChangeDir("data");
DVDOpen("stage1.dat", &file);             // files/data/stage1.dat

// Subdirectories
DVDOpen("data/levels/level1.dat", &file); // files/data/levels/level1.dat
```

## Custom Root Directory

By default, DVD uses `./files/` as the root. You can change this:

```c
// Load assets from different location
DVDSetRootDirectory("C:/GameAssets/MyGame/");

// Or relative path
DVDSetRootDirectory("../shared_assets/");

// Get current root
const char* root = DVDGetRootDirectory();
OSReport("DVD root: %s\n", root);
```

## File Organization Tips

### 1. Mirror Original Structure

Keep the same directory structure as the original game disc:

```
files/
├── data/
│   └── game_data.bin
├── audio/
│   └── music/
└── textures/
```

### 2. Group by Type

Organize assets by type for easier management:

```
files/
├── models/
├── textures/
├── audio/
├── scripts/
└── levels/
```

### 3. Support Modding

Allow users to replace files by placing them in `files/`:

```
files/
├── original/          ← Base game files
└── mods/             ← User modifications (check here first)
```

## Error Handling

```c
DVDFileInfo file;

if (!DVDOpen("missing.dat", &file)) {
    OSReport("File not found!\n");
    // Handle error
}

s32 result = DVDRead(&file, buffer, 1024, 0);
if (result < 0) {
    OSReport("Read error!\n");
} else if (result < 1024) {
    OSReport("Partial read: %d bytes\n", result);
}
```

## Performance Notes

### PC vs Original Hardware

| Operation | GameCube/Wii | PC (libPorpoise) |
|-----------|--------------|------------------|
| Open file | ~20-50ms | < 1ms |
| Read 1KB | ~10ms | < 1ms |
| Read 1MB | ~40ms | ~5ms (SSD) |
| Seek | ~40-100ms | < 1ms |

PC file access is **much faster** than disc reads. Games may need frame timing adjustments.

### Optimization Tips

1. **Use async reads** for large files
2. **Keep files open** if reading multiple times
3. **Read in chunks** for streaming
4. **Preload frequently accessed data** into RAM

## Common Patterns

### Loading Level Data

```c
// Load level file
DVDFileInfo levelFile;
void* levelData;

if (DVDOpen("levels/level1.dat", &levelFile)) {
    levelData = malloc(levelFile.length);
    DVDRead(&levelFile, levelData, levelFile.length, 0);
    DVDClose(&levelFile);
    
    // Parse and use levelData...
    free(levelData);
}
```

### Streaming Audio

```c
#define STREAM_BUFFER_SIZE (32 * 1024)
char audioBuffer[STREAM_BUFFER_SIZE];
s32 offset = 0;

DVDFileInfo audioFile;
DVDOpen("audio/music.dsp", &audioFile);

while (playing) {
    // Read next chunk
    s32 bytesRead = DVDRead(&audioFile, audioBuffer, STREAM_BUFFER_SIZE, offset);
    if (bytesRead <= 0) break;
    
    // Feed to audio system...
    offset += bytesRead;
}

DVDClose(&audioFile);
```

### Loading Texture Archive

```c
DVDFileInfo tplFile;
u8* textureData;

if (DVDOpen("textures/ui.tpl", &tplFile)) {
    textureData = malloc(tplFile.length);
    DVDRead(&tplFile, textureData, tplFile.length, 0);
    DVDClose(&tplFile);
    
    // Parse TPL format and load textures...
}
```

## Async Operation Example

```c
typedef struct {
    void* data;
    BOOL  loaded;
} AssetLoadState;

void AssetLoadedCallback(s32 result, DVDFileInfo* fileInfo) {
    AssetLoadState* state = (AssetLoadState*)fileInfo->callback;
    state->loaded = TRUE;
    OSReport("Asset loaded: %d bytes\n", result);
}

// Start async load
AssetLoadState state = { .data = malloc(1024 * 1024), .loaded = FALSE };
DVDFileInfo file;

if (DVDOpen("assets/big_texture.tpl", &file)) {
    file.callback = &state;
    DVDReadAsync(&file, state.data, file.length, 0, AssetLoadedCallback);
    
    // Do other work while loading...
    
    // Wait for completion
    while (!state.loaded) {
        OSSleepTicks(OSMillisecondsToTicks(1));
    }
    
    DVDClose(&file);
}
```

## Troubleshooting

**File not found:**
- Check `files/` directory exists
- Verify file path matches exactly (case-sensitive on Linux/macOS)
- Use forward slashes `/` in paths (converted automatically on Windows)
- Check DVDGetRootDirectory() to verify root path

**Read returns 0 bytes:**
- File may be empty
- Offset may be beyond end of file
- Check file permissions

**Async callback never called:**
- Ensure you're calling OSSleepTicks() or yielding to allow thread to run
- Check return value of DVDReadAsync() (FALSE = failed to start)
- Verify buffer pointer is valid for duration of read

## See Also

- [QUICKSTART.md](../QUICKSTART.md) - Getting started guide
- [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md) - Module status
- Example: `examples/dvd_test.c` - Complete working example

