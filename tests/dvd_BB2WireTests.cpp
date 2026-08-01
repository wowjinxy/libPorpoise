#include "../src/dvd/DVDWire.h"

#include <array>
#include <cstdint>
#include <iostream>

namespace {

bool Require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

}  // namespace

int main()
{
    static_assert(
        sizeof(DVDDecodedBB2) == DVD_BB2_WIRE_SIZE,
        "decoded BB2 fields must remain fixed-width"
    );

    const std::array<std::uint8_t, DVD_BB2_WIRE_SIZE> wire{{
        0x01, 0x23, 0x45, 0x67,
        0x89, 0xAB, 0xCD, 0xEF,
        0x00, 0x01, 0x23, 0x45,
        0x00, 0x02, 0x34, 0x56,
        0x81, 0x70, 0x00, 0x00,
        0x10, 0x20, 0x30, 0x40,
        0x50, 0x60, 0x70, 0x80,
        0xDE, 0xAD, 0xBE, 0xEF,
    }};
    DVDDecodedBB2 decoded{};

    if (!Require(
            __DVDDecodeBB2(wire.data(), wire.size(), &decoded),
            "canonical BB2 vector was rejected"
        ) ||
        !Require(
            decoded.bootFilePosition == 0x01234567u &&
                decoded.FSTPosition == 0x89ABCDEFu &&
                decoded.FSTLength == 0x00012345u &&
                decoded.FSTMaxLength == 0x00023456u &&
                decoded.FSTAddress == 0x81700000u &&
                decoded.userPosition == 0x10203040u &&
                decoded.userLength == 0x50607080u &&
                decoded.reserved == 0xDEADBEEFu,
            "BB2 fields were not decoded from big-endian wire order"
        ) ||
        !Require(
            !__DVDDecodeBB2(wire.data(), wire.size() - 1, &decoded) &&
                !__DVDDecodeBB2(nullptr, wire.size(), &decoded) &&
                !__DVDDecodeBB2(wire.data(), wire.size(), nullptr),
            "BB2 decoder accepted an invalid buffer"
        )) {
        return 1;
    }

    std::cout << "DVD BB2 big-endian wire decoding passed\n";
    return 0;
}
