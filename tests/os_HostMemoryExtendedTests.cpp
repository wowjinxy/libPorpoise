#include <dolphin/os.h>
#include <dolphin/os/OSHostMemory.h>

#include <cstddef>
#include <cstdint>

namespace {

constexpr std::size_t kBootInfoWireSize = 0x40;
constexpr std::size_t kBootInfoTailSize = 0x10;

bool Wire32Equals(const u8* wire, std::size_t offset, u32 value) {
    return wire[offset + 0] == static_cast<u8>(value >> 24) &&
           wire[offset + 1] == static_cast<u8>(value >> 16) &&
           wire[offset + 2] == static_cast<u8>(value >> 8) &&
           wire[offset + 3] == static_cast<u8>(value);
}

void SeedBootInfoTailCanary(u8* wire) {
    for (std::size_t index = 0; index < kBootInfoTailSize; ++index) {
        wire[kBootInfoWireSize + index] = static_cast<u8>(0xB0U + index);
    }
}

bool BootInfoTailCanaryIsIntact(const u8* wire) {
    for (std::size_t index = 0; index < kBootInfoTailSize; ++index) {
        if (wire[kBootInfoWireSize + index] !=
            static_cast<u8>(0xB0U + index)) {
            return false;
        }
    }
    return true;
}

bool BootInfoWireMatches(
    const u8* wire,
    const OSHostMemoryLayout* layout,
    u32 consoleType) {
    for (std::size_t index = 0; index < sizeof(DVDDiskID); ++index) {
        if (wire[index] != 0) {
            return false;
        }
    }

    return Wire32Equals(wire, 0x20, 0) &&
           Wire32Equals(wire, 0x24, 0) &&
           Wire32Equals(wire, 0x28, layout->consoleSize) &&
           Wire32Equals(wire, 0x2C, consoleType) &&
           Wire32Equals(
               wire,
               0x30,
               static_cast<u32>(
                   reinterpret_cast<std::uintptr_t>(layout->arenaLo))) &&
           Wire32Equals(
               wire,
               0x34,
               static_cast<u32>(
                   reinterpret_cast<std::uintptr_t>(layout->arenaHi))) &&
           Wire32Equals(wire, 0x38, 0) &&
           Wire32Equals(wire, 0x3C, 0);
}

}  // namespace

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
    if (!__OSHostMemoryContainsAddress(
            reinterpret_cast<void*>(0x81FFFFFFULL)) ||
        !__OSHostMemoryContainsAddress(
            reinterpret_cast<void*>(0xC1FFFFFFULL)) ||
        __OSHostMemoryContainsAddress(
            reinterpret_cast<void*>(0x82000000ULL)) ||
        __OSHostMemoryContainsAddress(
            reinterpret_cast<void*>(0xC2000000ULL))) {
        return 3;
    }
    if (OSGetPhysicalMemSize() != kConsoleSize ||
        OSGetConsoleSimulatedMemSize() != kConsoleSize) {
        return 4;
    }

    auto* cached = static_cast<volatile u32*>(
        OSPhysicalToCached(kExtendedSize - sizeof(u32)));
    auto* uncached = static_cast<volatile u32*>(
        OSPhysicalToUncached(kExtendedSize - sizeof(u32)));
    *cached = 0x13579BDFU;
    if (*uncached != 0x13579BDFU) {
        return 5;
    }
    *uncached = 0x2468ACE0U;
    if (*cached != 0x2468ACE0U) {
        return 6;
    }

    auto* bootInfoWire = static_cast<u8*>(OSPhysicalToCached(0));
    SeedBootInfoTailCanary(bootInfoWire);
    OSInit();
    if (!BootInfoWireMatches(bootInfoWire, layout, OSGetConsoleType()) ||
        !BootInfoTailCanaryIsIntact(bootInfoWire) ||
        OSGetArenaHi() != layout->arenaHi ||
        OSGetPhysicalMemSize() != kConsoleSize ||
        OSGetConsoleSimulatedMemSize() != kConsoleSize) {
        return 7;
    }

    // Native 64-bit ports need room beyond the authentic 24 MiB boundary for
    // pointer and vtable expansion. Exercise that compatibility space through
    // the SDK heap API, while the console-visible memory size stays unchanged.
    void* heapStart = OSInitAlloc(OSGetArenaLo(), OSGetArenaHi(), 1);
    const OSHeapHandle heap = OSCreateHeap(heapStart, OSGetArenaHi());
    void* allocation = OSAllocFromHeap(heap, kConsoleSize);
    if (heap < 0 || allocation == nullptr ||
        reinterpret_cast<uintptr_t>(allocation) + kConsoleSize <=
            reinterpret_cast<uintptr_t>(layout->consoleArenaHi)) {
        return 8;
    }
    OSFreeToHeap(heap, allocation);

    // A guest startup normalization from the initial extended high to the
    // authentic GameCube high preserves the host-only overhead exactly once.
    OSSetArenaHi(layout->consoleArenaHi);
    if (OSGetArenaHi() != layout->arenaHi) {
        return 9;
    }

    // The adjustment is consumed; subsequent heap operations are exact.
    OSSetArenaHi(layout->consoleArenaHi);
    if (OSGetArenaHi() != layout->consoleArenaHi) {
        return 10;
    }
    void* laterHeapHigh = reinterpret_cast<void*>(0x81000000ULL);
    OSSetArenaHi(laterHeapHigh);
    return OSGetArenaHi() == laterHeapHigh ? 0 : 11;
}
