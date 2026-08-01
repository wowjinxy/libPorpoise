#include <array>
#include <cstddef>

#include <dolphin/gx/GXTypes.h>
#include <dolphin/gx/GXFifo.h>

namespace {

std::array<u8, 16> gFifoBytes = {};
size_t gFifoSize = 0;
bool gUsedNumericScalarWrite = false;

void Record(u8 value) {
    if (gFifoSize < gFifoBytes.size()) {
        gFifoBytes[gFifoSize++] = value;
    }
}

}

extern "C" void SIM_GX_CommandProcessor_SendU8(u8 value) {
    Record(value);
}

extern "C" void SIM_GX_CommandProcessor_SendU16(u16 value) {
    Record(static_cast<u8>(value));
    Record(static_cast<u8>(value >> 8));
}

extern "C" void SIM_GX_CommandProcessor_SendU32(u32) {
    gUsedNumericScalarWrite = true;
}

int main() {
    static_assert(sizeof(GXColor) == 4u);
    const GXColor components = {0x12u, 0x34u, 0x56u, 0x78u};
    if (GXColorToRGBA8(components) != 0x12345678u ||
        GXCOLOR_AS_U32(components) != 0x12345678u) {
        return 1;
    }
    const GXColor unpacked = GXColorFromRGBA8(0x89abcdefu);
    if (unpacked.r != 0x89u || unpacked.g != 0xabu ||
        unpacked.b != 0xcdu || unpacked.a != 0xefu) {
        return 2;
    }

    GXColor1u16(0x1234u);
    GXColor1u32(0x56789abcu);
    GXColor4u8(0xdeu, 0xadu, 0xbeu, 0xefu);
    GXColor1u32(GXColorToRGBA8(components));

    constexpr std::array<u8, 14> expected = {
        0x34u, 0x12u,
        0x56u, 0x78u, 0x9au, 0xbcu,
        0xdeu, 0xadu, 0xbeu, 0xefu,
        0x12u, 0x34u, 0x56u, 0x78u,
    };
    if (gUsedNumericScalarWrite || gFifoSize != expected.size()) {
        return 3;
    }
    for (size_t index = 0; index < expected.size(); ++index) {
        if (gFifoBytes[index] != expected[index]) {
            return 4;
        }
    }
    return 0;
}
