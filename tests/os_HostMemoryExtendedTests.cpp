#include <dolphin/os.h>
#include <dolphin/os/OSHostMemory.h>

#include <cstdint>

extern "C" void SIM_VIInit(void) {
}

extern "C" void SIM_Render(void) {
}

int main() {
    constexpr u32 kExtendedSize = 0x02000000U;
    constexpr u32 kConsoleSize = 0x01800000U;
    const OSHostMemoryLayout* layout =
        __OSHostMemoryInit(OS_HOST_MEMORY_PROFILE_GAMECUBE_EXTENDED);

    if (layout == nullptr ||
        layout->profile != OS_HOST_MEMORY_PROFILE_GAMECUBE_EXTENDED ||
        layout->cachedBase != reinterpret_cast<void*>(0x80000000ULL) ||
        layout->uncachedBase != reinterpret_cast<void*>(0xC0000000ULL) ||
        layout->size != kExtendedSize ||
        layout->consoleSize != kConsoleSize ||
        layout->arenaLo != reinterpret_cast<void*>(0x80004000ULL) ||
        layout->arenaHi != reinterpret_cast<void*>(0x81FFFA80ULL) ||
        layout->consoleArenaHi != reinterpret_cast<void*>(0x817FFA80ULL)) {
        return 1;
    }
    if (__OSHostMemoryGetLayout() != layout ||
        __OSHostMemoryInit(OS_HOST_MEMORY_PROFILE_GAMECUBE_EXTENDED) != layout) {
        return 2;
    }
    if (OSGetPhysicalMemSize() != kConsoleSize ||
        OSGetConsoleSimulatedMemSize() != kConsoleSize) {
        return 3;
    }

    auto* cached = static_cast<volatile u32*>(
        OSPhysicalToCached(kExtendedSize - sizeof(u32)));
    auto* uncached = static_cast<volatile u32*>(
        OSPhysicalToUncached(kExtendedSize - sizeof(u32)));
    *cached = 0x13579BDFU;
    if (*uncached != 0x13579BDFU) {
        return 4;
    }
    *uncached = 0x2468ACE0U;
    if (*cached != 0x2468ACE0U) {
        return 5;
    }

    OSInit();
    const auto* bootInfo =
        reinterpret_cast<const OSBootInfo*>(0x80000000ULL);
    if (bootInfo->memorySize != kConsoleSize ||
        bootInfo->arenaHi != layout->arenaHi ||
        OSGetArenaHi() != layout->arenaHi ||
        OSGetPhysicalMemSize() != kConsoleSize ||
        OSGetConsoleSimulatedMemSize() != kConsoleSize) {
        return 6;
    }

    // A guest startup normalization from the initial extended high to the
    // authentic GameCube high preserves the host-only overhead exactly once.
    OSSetArenaHi(layout->consoleArenaHi);
    if (OSGetArenaHi() != layout->arenaHi) {
        return 7;
    }

    // The adjustment is consumed; subsequent heap operations are exact.
    OSSetArenaHi(layout->consoleArenaHi);
    if (OSGetArenaHi() != layout->consoleArenaHi) {
        return 8;
    }
    void* laterHeapHigh = reinterpret_cast<void*>(0x81000000ULL);
    OSSetArenaHi(laterHeapHigh);
    return OSGetArenaHi() == laterHeapHigh ? 0 : 9;
}
