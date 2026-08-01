#include <dolphin/ar.h>

#include <array>

extern "C" void SIM_VIInit(void) {
}

extern "C" void SIM_Render(void) {
}

int main() {
    constexpr u32 Base = __AR_ARAM_USR_BASE_ADDR;
    constexpr u32 Alignment = ARQ_DMA_ALIGNMENT;

    ARReset();
    u32 freedLength = 0xFFFFFFFFU;
    if (ARCheckInit() || ARAlloc(Alignment) != 0 ||
        ARFree(&freedLength) != 0 || freedLength != 0) {
        return 1;
    }

    std::array<u32, 3> stack = {};
    std::array<u32, 1> ignoredStack = {};
    if (ARInit(stack.data(), static_cast<u32>(stack.size())) != Base ||
        !ARCheckInit() || ARGetBaseAddress() != Base ||
        ARGetSize() != 0x01000000U) {
        return 2;
    }

    const u32 first = ARAlloc(32);
    if (first != Base || (first & (Alignment - 1U)) != 0 || stack[0] != 32) {
        return 3;
    }

    /* ARInit is idempotent and must not replace or reset an active stack. */
    if (ARInit(ignoredStack.data(), 1) != Base || ARAlloc(33) != 0) {
        return 4;
    }

    const u32 second = ARAlloc(64);
    const u32 third = ARAlloc(96);
    if (second != Base + 32 || third != Base + 96 ||
        (second & (Alignment - 1U)) != 0 ||
        (third & (Alignment - 1U)) != 0 ||
        stack[1] != 64 || stack[2] != 96 || ignoredStack[0] != 0) {
        return 5;
    }

    /* Exhausting the index table must not mutate allocator state. */
    if (ARAlloc(Alignment) != 0 || ARFree(&freedLength) != third ||
        freedLength != 96) {
        return 6;
    }

    /* An oversized aligned request must be rejected without integer wrap. */
    const u32 oversized = ARGetSize() - Base + Alignment;
    if (ARAlloc(oversized) != 0 || ARAlloc(96) != third) {
        return 7;
    }

    if (ARFree(&freedLength) != third || freedLength != 96 ||
        ARFree(&freedLength) != second || freedLength != 64 ||
        ARFree(&freedLength) != first || freedLength != 32) {
        return 8;
    }

    freedLength = 0xFFFFFFFFU;
    if (ARFree(&freedLength) != 0 || freedLength != 0 ||
        ARAlloc(Alignment) != Base) {
        return 9;
    }

    ARReset();
    freedLength = 0xFFFFFFFFU;
    if (ARCheckInit() || ARAlloc(Alignment) != 0 ||
        ARFree(&freedLength) != 0 || freedLength != 0) {
        return 10;
    }

    std::array<u32, 1> reinitStack = {};
    if (ARInit(reinitStack.data(), 1) != Base ||
        ARAlloc(ARGetSize() - Base) != Base ||
        reinitStack[0] != ARGetSize() - Base ||
        ARAlloc(Alignment) != 0) {
        return 11;
    }

    if (ARFree(&freedLength) != Base ||
        freedLength != ARGetSize() - Base) {
        return 12;
    }

    ARReset();
    if (ARInit(nullptr, 0) != Base || ARAlloc(Alignment) != 0) {
        return 13;
    }

    return 0;
}
