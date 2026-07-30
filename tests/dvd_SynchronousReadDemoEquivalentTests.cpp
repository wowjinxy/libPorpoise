#include "sdk_DvdFixture.hpp"

#include <dolphin/dvd.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

bool Check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

}  // namespace

int main()
{
    const std::vector<std::uint8_t> expected{
        'L', 'i', 'b', 'P', 'o', 'r', 'p', 'o', 'i', 's', 'e', ' ',
        's', 'y', 'n', 'c', 'h', 'r', 'o', 'n', 'o', 'u', 's', ' ',
        'D', 'V', 'D', ' ', 'r', 'e', 'a', 'd', '\n',
    };

    DvdFixture fixture("sdk-030-dvddemo1");
    if (!Check(fixture.valid(), "temporary DVD root creation failed") ||
        !Check(
            fixture.write("texts/test1.txt", expected),
            "test1.txt creation failed"
        ) ||
        !Check(fixture.initialize(), "DVD host initialization failed")) {
        return 1;
    }

    DVDFileInfo file{};
    if (!Check(DVDOpen("texts/test1.txt", &file), "DVDOpen failed") ||
        !Check(
            DVDGetLength(&file) == expected.size(),
            "DVDGetLength returned the wrong size"
        )) {
        return 1;
    }

    alignas(32) std::array<std::uint8_t, 64> buffer{};
    std::fill(buffer.begin(), buffer.end(), 0xCD);
    const s32 roundedLength =
        static_cast<s32>((DVDGetLength(&file) + 31u) & ~31u);
    const s32 result = DVDRead(&file, buffer.data(), roundedLength, 0);

    if (!Check(result >= 0, "synchronous DVDRead failed") ||
        !Check(
            std::memcmp(buffer.data(), expected.data(), expected.size()) == 0,
            "synchronous DVDRead returned incorrect file bytes"
        ) ||
        !Check(
            DVDGetTransferredSize(&file) == result,
            "transferred-size accounting differs from DVDRead result"
        ) ||
        !Check(DVDGetFileInfoStatus(&file) == DVD_STATE_END, "read did not end") ||
        !Check(DVDClose(&file), "DVDClose failed")) {
        return 1;
    }

    DVDFileInfo missing{};
    if (!Check(
            !DVDOpen("texts/missing.txt", &missing),
            "DVDOpen accepted a missing file"
        )) {
        return 1;
    }

    std::cout << "DVD synchronous open/read/close passed\n";
    return 0;
}
