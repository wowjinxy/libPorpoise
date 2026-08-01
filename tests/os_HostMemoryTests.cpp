#include <dolphin/os.h>
#include <dolphin/hw_regs.h>
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
        wire[kBootInfoWireSize + index] = static_cast<u8>(0xA0U + index);
    }
}

bool BootInfoTailCanaryIsIntact(const u8* wire) {
    for (std::size_t index = 0; index < kBootInfoTailSize; ++index) {
        if (wire[kBootInfoWireSize + index] !=
            static_cast<u8>(0xA0U + index)) {
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
    const OSHostMemoryLayout* layout =
        __OSHostMemoryInit(OS_HOST_MEMORY_PROFILE_GAMECUBE);

    if (layout == nullptr ||
        layout->cachedBase != reinterpret_cast<void*>(0x80000000ULL) ||
        layout->uncachedBase != reinterpret_cast<void*>(0xC0000000ULL) ||
        layout->size != 0x01800000U ||
        layout->consoleSize != 0x01800000U ||
        layout->arenaLo != reinterpret_cast<void*>(0x80004000ULL) ||
        layout->arenaHi != reinterpret_cast<void*>(0x817FFA80ULL) ||
        layout->consoleArenaHi != reinterpret_cast<void*>(0x817FFA80ULL)) {
        return 1;
    }
    if (__OSHostMemoryGetLayout() != layout ||
        __OSHostMemoryInit(OS_HOST_MEMORY_PROFILE_GAMECUBE) != layout) {
        return 2;
    }
    if (!__OSHostMemoryContainsAddress(
            reinterpret_cast<void*>(0x80000000ULL)) ||
        !__OSHostMemoryContainsAddress(
            reinterpret_cast<void*>(0x817FFFFFULL)) ||
        !__OSHostMemoryContainsAddress(
            reinterpret_cast<void*>(0xC0000000ULL)) ||
        !__OSHostMemoryContainsAddress(
            reinterpret_cast<void*>(0xC17FFFFFULL)) ||
        __OSHostMemoryContainsAddress(nullptr) ||
        __OSHostMemoryContainsAddress(
            reinterpret_cast<void*>(0x81800000ULL)) ||
        __OSHostMemoryContainsAddress(
            reinterpret_cast<void*>(0xC1800000ULL))) {
        return 3;
    }
    if (OSPhysicalToCached(0x123456) !=
            reinterpret_cast<void*>(0x80123456ULL) ||
        OSPhysicalToUncached(0x123456) !=
            reinterpret_cast<void*>(0xC0123456ULL)) {
        return 4;
    }

    auto* cached = static_cast<volatile u32*>(
        OSPhysicalToCached(0x00100000));
    auto* uncached = static_cast<volatile u32*>(
        OSPhysicalToUncached(0x00100000));
    *cached = 0x1234ABCDU;
    if (*uncached != 0x1234ABCDU) {
        return 5;
    }
    *uncached = 0x89ABCDEFU;
    if (*cached != 0x89ABCDEFU) {
        return 6;
    }

    auto* bootInfoWire = static_cast<u8*>(OSPhysicalToCached(0));
    SeedBootInfoTailCanary(bootInfoWire);
    OSInit();
    return BootInfoWireMatches(bootInfoWire, layout, OSGetConsoleType()) &&
                   BootInfoTailCanaryIsIntact(bootInfoWire) &&
                   __DSPRegs[DSP_ARAM_DMA_MM_HI] == 0x0100U &&
                   __DSPRegs[DSP_ARAM_DMA_MM_LO] == 0x0000U &&
                   __DSPRegs[DSP_ARAM_DMA_ARAM_HI] == 0x0000U &&
                   __DSPRegs[DSP_ARAM_DMA_ARAM_LO] == 0x0000U &&
                   __DSPRegs[DSP_ARAM_DMA_SIZE_HI] == 0x0000U &&
                   __DSPRegs[DSP_ARAM_DMA_SIZE_LO] == 0x0020U &&
                   OSGetPhysicalMemSize() == 0x01800000U &&
                   OSGetConsoleSimulatedMemSize() == 0x01800000U &&
                   OSGetArenaLo() == layout->arenaLo &&
                   OSGetArenaHi() == layout->arenaHi &&
                   OSCheckActiveThreads() == 1
               ? 0
               : 7;
}
