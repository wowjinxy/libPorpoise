#include <revolution/arc.h>
#include <revolution/darch.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

std::uint32_t ReadBe32(const std::uint8_t* bytes)
{
    return (static_cast<std::uint32_t>(bytes[0]) << 24) |
           (static_cast<std::uint32_t>(bytes[1]) << 16) |
           (static_cast<std::uint32_t>(bytes[2]) << 8) |
           static_cast<std::uint32_t>(bytes[3]);
}

bool Check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

bool VerifyFile(
    ARCHandle& handle,
    const char* path,
    const std::vector<std::uint8_t>& expected
)
{
    ARCFileInfo opened{};
    if (!Check(ARCOpen(&handle, path, &opened), "ARCOpen failed") ||
        !Check(ARCGetLength(&opened) == expected.size(), "file length differs")) {
        return false;
    }
    const void* data = ARCGetStartAddrInMem(&opened);
    return Check(
        expected.empty() ||
            std::memcmp(data, expected.data(), expected.size()) == 0,
        "file bytes differ"
    );
}

}  // namespace

int main()
{
    const std::vector<std::uint8_t> hero{0x48, 0x45, 0x52, 0x4F};
    const std::vector<std::uint8_t> rival{0x52, 0x49, 0x56, 0x41, 0x4C};
    const std::vector<std::uint8_t> city(37, 0xC7);
    const std::vector<std::uint8_t> readme{
        'p', 'o', 'r', 'p', 'o', 'i', 's', 'e', '\n'
    };
    const std::vector<std::uint8_t> empty;

    const std::array<DARCHFileInfo, 5> files{{
        {"art/characters/hero.tpl", hero.data(), static_cast<u32>(hero.size())},
        {"art/characters/rival.tpl", rival.data(), static_cast<u32>(rival.size())},
        {"art/backgrounds/city.bmp", city.data(), static_cast<u32>(city.size())},
        {"docs/readme.txt", readme.data(), static_cast<u32>(readme.size())},
        {"docs/empty.bin", nullptr, 0},
    }};

    const u32 archiveSize =
        DARCHGetArcSize(files.data(), static_cast<u32>(files.size()));
    if (!Check(archiveSize != 0, "archive sizing failed")) {
        return 1;
    }

    std::vector<std::uint8_t> archive(archiveSize);
    std::vector<std::uint8_t> duplicate(archiveSize);
    if (!Check(
            DARCHCreate(
                archive.data(),
                static_cast<u32>(archive.size()),
                files.data(),
                static_cast<u32>(files.size())
            ),
            "archive creation failed"
        ) ||
        !Check(
            DARCHCreate(
                duplicate.data(),
                static_cast<u32>(duplicate.size()),
                files.data(),
                static_cast<u32>(files.size())
            ),
            "repeat archive creation failed"
        ) ||
        !Check(archive == duplicate, "archive output is not deterministic") ||
        !Check(
            !DARCHCreate(
                duplicate.data(),
                static_cast<u32>(duplicate.size() - 1),
                files.data(),
                static_cast<u32>(files.size())
            ),
            "undersized output buffer was accepted"
        )) {
        return 1;
    }

    ARCHandle handle{};
    if (!Check(ARCInitHandle(archive.data(), &handle), "ARC rejected DARCH output") ||
        !VerifyFile(handle, "/art/characters/hero.tpl", hero) ||
        !VerifyFile(handle, "art/characters/rival.tpl", rival) ||
        !VerifyFile(handle, "art/backgrounds/city.bmp", city) ||
        !VerifyFile(handle, "docs/readme.txt", readme) ||
        !VerifyFile(handle, "docs/empty.bin", empty)) {
        return 1;
    }

    const std::uint32_t fstOffset = ReadBe32(archive.data() + 4);
    const std::uint32_t nodeCount = ReadBe32(archive.data() + fstOffset + 8);
    for (std::uint32_t i = 1; i < nodeCount; ++i) {
        const std::uint8_t* entry = archive.data() + fstOffset + i * 12;
        if ((ReadBe32(entry) & 0xFF000000u) == 0 &&
            !Check(
                ReadBe32(entry + 4) % 32 == 0,
                "file offset is not 32-byte aligned"
            )) {
            return 1;
        }
    }

    const std::array<DARCHFileInfo, 2> duplicatePaths{{
        {"same.bin", hero.data(), static_cast<u32>(hero.size())},
        {"same.bin", rival.data(), static_cast<u32>(rival.size())},
    }};
    const std::array<DARCHFileInfo, 2> caseOnlyDuplicatePaths{{
        {"Case/Save.bin", hero.data(), static_cast<u32>(hero.size())},
        {"case/save.BIN", rival.data(), static_cast<u32>(rival.size())},
    }};
    if (!Check(
            DARCHGetArcSize(
                duplicatePaths.data(),
                static_cast<u32>(duplicatePaths.size())
            ) == 0,
            "duplicate path was accepted"
        ) ||
        !Check(
            DARCHGetArcSize(
                caseOnlyDuplicatePaths.data(),
                static_cast<u32>(caseOnlyDuplicatePaths.size())
            ) == 0,
            "ASCII case-insensitive duplicate path was accepted"
        )) {
        return 1;
    }

    /*
     * Insert directories before files to make the expected SDK ordering
     * observable: every directory emits its files first, then child dirs.
     */
    const std::array<DARCHFileInfo, 5> orderFiles{{
        {"zdir/sub/deep.bin", hero.data(), static_cast<u32>(hero.size())},
        {"root.bin", rival.data(), static_cast<u32>(rival.size())},
        {"zdir/direct.bin", city.data(), static_cast<u32>(city.size())},
        {"adir/leaf.bin", readme.data(), static_cast<u32>(readme.size())},
        {"second.bin", nullptr, 0},
    }};
    const u32 orderArchiveSize = DARCHGetArcSize(
        orderFiles.data(),
        static_cast<u32>(orderFiles.size())
    );
    std::vector<std::uint8_t> orderArchive(orderArchiveSize);
    if (!Check(orderArchiveSize != 0, "ordered archive sizing failed") ||
        !Check(
            DARCHCreate(
                orderArchive.data(),
                static_cast<u32>(orderArchive.size()),
                orderFiles.data(),
                static_cast<u32>(orderFiles.size())
            ),
            "ordered archive creation failed"
        )) {
        return 1;
    }

    const std::array<const char*, 9> expectedNames{{
        "", "root.bin", "second.bin", "zdir", "direct.bin",
        "sub", "deep.bin", "adir", "leaf.bin",
    }};
    const std::array<bool, 9> expectedDirectories{{
        true, false, false, true, false, true, false, true, false,
    }};
    const std::uint32_t orderFstOffset = ReadBe32(orderArchive.data() + 4);
    const std::uint8_t* orderFst = orderArchive.data() + orderFstOffset;
    const std::uint32_t orderNodeCount = ReadBe32(orderFst + 8);
    if (!Check(
            orderNodeCount == expectedNames.size(),
            "SDK-ordered archive has the wrong node count"
        )) {
        return 1;
    }
    const char* orderStrings = reinterpret_cast<const char*>(
        orderFst + orderNodeCount * 12u
    );
    for (std::uint32_t i = 0; i < orderNodeCount; ++i) {
        const std::uint32_t typeAndName = ReadBe32(orderFst + i * 12u);
        const bool isDirectory = (typeAndName & 0xFF000000u) != 0;
        const char* name = orderStrings + (typeAndName & 0x00FFFFFFu);
        if (!Check(
                isDirectory == expectedDirectories[i] &&
                    std::strcmp(name, expectedNames[i]) == 0,
                "DARCH node order differs from SDK files-before-dirs order"
            )) {
            return 1;
        }
    }

    std::cout << "DARCH archive construction round-trip passed\n";
    return 0;
}
