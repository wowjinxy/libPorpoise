#include <dolphin/os.h>
#include <dolphin/os/OSHostMemory.h>

#include <cstdint>

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
        layout->arenaLo != reinterpret_cast<void*>(0x80004000ULL) ||
        layout->arenaHi != reinterpret_cast<void*>(0x817FFA80ULL)) {
        return 1;
    }
    if (__OSHostMemoryGetLayout() != layout ||
        __OSHostMemoryInit(OS_HOST_MEMORY_PROFILE_GAMECUBE) != layout) {
        return 2;
    }
    if (OSPhysicalToCached(0x123456) !=
            reinterpret_cast<void*>(0x80123456ULL) ||
        OSPhysicalToUncached(0x123456) !=
            reinterpret_cast<void*>(0xC0123456ULL)) {
        return 3;
    }

    auto* cached = static_cast<volatile u32*>(
        OSPhysicalToCached(0x00100000));
    auto* uncached = static_cast<volatile u32*>(
        OSPhysicalToUncached(0x00100000));
    *cached = 0x1234ABCDU;
    if (*uncached != 0x1234ABCDU) {
        return 4;
    }
    *uncached = 0x89ABCDEFU;
    if (*cached != 0x89ABCDEFU) {
        return 5;
    }

    OSInit();
    const auto* bootInfo =
        reinterpret_cast<const OSBootInfo*>(0x80000000ULL);
    return bootInfo->memorySize == 0x01800000U &&
                   bootInfo->arenaLo == layout->arenaLo &&
                   bootInfo->arenaHi == layout->arenaHi &&
                   OSGetArenaLo() == layout->arenaLo &&
                   OSGetArenaHi() == layout->arenaHi
               ? 0
               : 6;
}
