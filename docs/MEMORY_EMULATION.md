# Memory Emulation Modes

libPorpoise supports two memory emulation modes to accommodate different porting needs.

## Mode 1: Simple Mode (Default)

**Best for:** Most game ports, simple conversions

**Characteristics:**
- ✅ Fast and lightweight
- ✅ Cache operations are no-ops
- ✅ Works with standard memory allocation
- ❌ No locked cache support
- ❌ No address translation

**Build:**
```bash
cmake ..
cmake --build .
```

**Usage:**
```c
#include <dolphin/os.h>

void* buffer = OSAlloc(1024);
DCFlushRange(buffer, 1024);  // No-op, but API compatible
```

---

## Mode 2: Full Emulation Mode

**Best for:** Games using locked cache, precise hardware emulation, testing

**Characteristics:**
- ✅ Complete memory layout emulation
- ✅ Locked cache support (0xE0000000 range)
- ✅ Address translation (0x80000000, 0xC0000000 mirrors)
- ✅ Big-endian byte order handling
- ⚠️ Slightly more overhead

**Build:**
```bash
cmake .. -DPORPOISE_USE_GECKO_MEMORY=ON
cmake --build .
```

**Usage:**
```c
#include <dolphin/os.h>
#include <dolphin/gecko_memory.h>

// Initialize full memory emulation
GeckoMemory memory;
GeckoMemoryInit(&memory, TRUE); // TRUE = Wii mode
g_geckoMemory = &memory;

// Now locked cache works!
LCEnable();
LCAlloc(0xE0000000, 4096);

// Use locked cache
void* audioBuffer = (void*)0xE0000000;
LCLoadData(audioBuffer, sourceData, 4096);
ProcessAudio(audioBuffer); // Process in fast scratchpad
LCStoreData(destData, audioBuffer, 4096);
```

---

## Memory Layout (Full Emulation Mode)

### Virtual Address Ranges

| Range | Description | Maps To |
|-------|-------------|---------|
| `0x00000000 - 0x017FFFFF` | Physical MEM1 | MEM1 (24 MB) |
| `0x80000000 - 0x817FFFFF` | Cached MEM1 | MEM1 (mirror) |
| `0xC0000000 - 0xC17FFFFF` | Uncached MEM1 | MEM1 (mirror) |
| `0x90000000 - 0x93FFFFFF` | Cached MEM2 (Wii) | MEM2 (64 MB) |
| `0xD0000000 - 0xD3FFFFFF` | Uncached MEM2 (Wii) | MEM2 (mirror) |
| `0xE0000000 - 0xE0003FFF` | Locked Cache | 16 KB scratchpad |
| `0xCC000000 - 0xCCFFFFFF` | Hardware Regs | (future) |

### Example: Address Translation

```c
// All these refer to the same physical location:
u32 addr_physical = 0x00001000;
u32 addr_cached   = 0x80001000;
u32 addr_uncached = 0xC0001000;

GeckoWrite32(&memory, addr_cached, 0xDEADBEEF);
u32 value = GeckoRead32(&memory, addr_uncached); // Returns 0xDEADBEEF
```

---

## Locked Cache Details

**What is Locked Cache?**

On GameCube/Wii, the CPU has 32 KB of L1 data cache. 16 KB can be "locked" - used as ultra-fast scratchpad RAM instead of cache.

**Original Hardware:**
- Access via special address: `0xE0000000 - 0xE0003FFF`
- Has DMA engine for fast transfers
- ~2x faster than main RAM
- Used for audio processing, vertex transforms, compression

**Porpoise Emulation:**
- Simple mode: Not available
- Full emulation: 16 KB buffer that works like the real thing
- DMA operations become instant memcpy

**Common Usage Pattern:**

```c
// Enable locked cache
LCEnable();

// Allocate 4 KB at 0xE0000000
LCAlloc(0xE0000000, 4096);

// Load data from main RAM to locked cache (DMA)
void* lcBuffer = (void*)0xE0000000;
LCLoadData(lcBuffer, mainRamData, 4096);

// Process in scratchpad (fast!)
ProcessData(lcBuffer);

// Store back to main RAM (DMA)
LCStoreData(mainRamResult, lcBuffer, 4096);

// Disable when done
LCDisable();
```

---

## Which Mode Should You Use?

### Use Simple Mode (default) if:
- ✅ Your game doesn't use locked cache
- ✅ You want fastest build/smallest footprint
- ✅ Basic port is sufficient

### Use Full Emulation if:
- ✅ Game uses locked cache (`LCEnable`, `LCLoadData`, etc.)
- ✅ Game accesses `0xE0000000` address range
- ✅ Need precise memory layout for testing
- ✅ Porting audio/graphics code that uses scratchpad

---

## Performance Impact

**Simple Mode:**
- Negligible overhead
- Cache functions compile to empty code

**Full Emulation Mode:**
- ~88 MB RAM for simulated memory (24 MB + 64 MB)
- Locked cache operations are instant memcpy (no DMA wait)
- Address translation is simple bit masking (very fast)
- Overall: <5% performance impact

---

## Example: Building with Full Emulation

**CMake:**
```bash
cmake .. -DPORPOISE_USE_GECKO_MEMORY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

**Manual Define:**
```c
// In your game's CMakeLists.txt:
add_definitions(-DPORPOISE_USE_GECKO_MEMORY)
```

---

## Implementation Notes

### Memory Access Functions

When full emulation is enabled, you can use:

```c
#include <dolphin/gecko_memory.h>

// Read/write with address translation and big-endian conversion
u32 value = GeckoRead32(g_geckoMemory, 0x80001000);
GeckoWrite32(g_geckoMemory, 0x80001000, 0x12345678);

// Get direct pointer (for performance)
void* ptr = GeckoGetPointer(g_geckoMemory, 0x80001000);
memcpy(ptr, data, size);
```

### Initialization

```c
int main(void) {
    // Simple mode
    OSInit();
    
    // OR full emulation mode:
#ifdef PORPOISE_USE_GECKO_MEMORY
    GeckoMemory memory;
    GeckoMemoryInit(&memory, TRUE); // Wii mode
    g_geckoMemory = &memory;
    
    OSInit();
    
    // ... your game code ...
    
    GeckoMemoryFree(&memory);
#endif
    
    return 0;
}
```

---

## Future Enhancements

When full emulation is enabled, we could add:
- Hardware register emulation
- Real DMA queue simulation
- Memory protection ranges
- Performance counters

Stay tuned for updates!

