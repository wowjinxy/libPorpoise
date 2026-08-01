#include <dolphin/dvd.h>
#include <dolphin/os.h>
#include <dolphin/os/OSHostEndian.h>
#include <dolphin/tpl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

struct NativeFSTEntry {
    std::uint32_t isDirectoryAndNameOffset;
    std::uint32_t parentOrPosition;
    std::uint32_t nextEntryOrLength;
};

static_assert(sizeof(NativeFSTEntry) == 12);

bool require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

}  // namespace

int runHostFilesystemTest() {
    namespace fs = std::filesystem;

    const auto unique =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root =
        fs::temp_directory_path() /
        ("libporpoise-dvd-host-" + std::to_string(unique));
    const fs::path testDirectory = root / "gxTests";
    const fs::path testFile = testDirectory / "sample.bin";
    const fs::path paddedReadFile = testDirectory / "padded-read.bin";
    const fs::path validTplFile = testDirectory / "valid.tpl";
    const fs::path truncatedTplFile = testDirectory / "truncated.tpl";
    const fs::path nestedDirectory = testDirectory / "nested";
    const fs::path deeperDirectory = nestedDirectory / "deeper";
    const fs::path deepFile = deeperDirectory / "deep.bin";
    const fs::path leafFile = nestedDirectory / "leaf.bin";
    const std::array<std::uint8_t, 8> expected{
        0x47, 0x58, 0x20, 0x44, 0x56, 0x44, 0x00, 0xff};
    const std::array<std::uint8_t, 5> deepExpected{
        0xde, 0xad, 0xbe, 0xef, 0x42};
    const std::array<std::uint8_t, 3> leafExpected{0x10, 0x20, 0x30};
    std::array<std::uint8_t, 120> paddedExpected{};
    for (std::size_t i = 0; i < paddedExpected.size(); ++i) {
        paddedExpected[i] = static_cast<std::uint8_t>((i * 37u + 11u) & 0xffu);
    }
    std::array<std::uint8_t, 88> validTpl{};
    OSWriteBigEndian32(validTpl.data() + 0, 0x0020af30u);
    OSWriteBigEndian32(validTpl.data() + 4, 1u);
    OSWriteBigEndian32(validTpl.data() + 8, 12u);
    OSWriteBigEndian32(validTpl.data() + 12, 20u);
    OSWriteBigEndian16(validTpl.data() + 20, 8u);
    OSWriteBigEndian16(validTpl.data() + 22, 8u);
    OSWriteBigEndian32(validTpl.data() + 24, GX_TF_I4);
    OSWriteBigEndian32(validTpl.data() + 28, 56u);
    std::fill(validTpl.begin() + 56, validTpl.end(), 0xabu);

    std::error_code error;
    fs::create_directories(deeperDirectory, error);
    if (!require(!error, "failed to create temporary DVD root")) return 1;

    {
        std::ofstream output(testFile, std::ios::binary);
        output.write(
            reinterpret_cast<const char*>(expected.data()),
            static_cast<std::streamsize>(expected.size()));
    }
    {
        std::ofstream output(paddedReadFile, std::ios::binary);
        output.write(
            reinterpret_cast<const char*>(paddedExpected.data()),
            static_cast<std::streamsize>(paddedExpected.size()));
    }
    {
        std::ofstream output(validTplFile, std::ios::binary);
        output.write(
            reinterpret_cast<const char*>(validTpl.data()),
            static_cast<std::streamsize>(validTpl.size()));
    }
    {
        std::ofstream output(truncatedTplFile, std::ios::binary);
        output.write(
            reinterpret_cast<const char*>(validTpl.data()),
            57);
    }
    {
        std::ofstream output(deepFile, std::ios::binary);
        output.write(
            reinterpret_cast<const char*>(deepExpected.data()),
            static_cast<std::streamsize>(deepExpected.size()));
    }
    {
        std::ofstream output(leafFile, std::ios::binary);
        output.write(
            reinterpret_cast<const char*>(leafExpected.data()),
            static_cast<std::streamsize>(leafExpected.size()));
    }

    const std::string rootString = root.string();
    if (!require(
            DVDSetRootDirectory(rootString.c_str()) == TRUE,
            "DVDSetRootDirectory rejected a valid host path")) {
        fs::remove_all(root, error);
        return 1;
    }

    OSInit();
    DVDInit();

    const auto* fst = static_cast<const NativeFSTEntry*>(DVDGetFSTLocation());
    if (!require(fst != nullptr, "DVDGetFSTLocation returned null on host") ||
        !require(
            fst[0].nextEntryOrLength == 10u,
            "host FST root did not expose the complete recursive snapshot")) {
        fs::remove_all(root, error);
        return 1;
    }
    const char* fstNames = reinterpret_cast<const char*>(fst + 10);
    const std::array<const char*, 10> expectedFstNames{
        "",
        "gxTests",
        "nested",
        "deeper",
        "deep.bin",
        "leaf.bin",
        "padded-read.bin",
        "sample.bin",
        "truncated.tpl",
        "valid.tpl",
    };
    for (std::uint32_t entryNum = 0; entryNum < expectedFstNames.size();
         ++entryNum) {
        const std::uint32_t nameOffset =
            fst[entryNum].isDirectoryAndNameOffset & 0x00ffffffu;
        if (!require(
                std::string(fstNames + nameOffset) ==
                    expectedFstNames[entryNum],
                "host FST string table/order was not deterministic")) {
            fs::remove_all(root, error);
            return 1;
        }
    }
    if (!require(
            fst[0].isDirectoryAndNameOffset == 0x01000000u &&
                fst[0].parentOrPosition == 0u &&
                fst[1].parentOrPosition == 0u &&
                fst[1].nextEntryOrLength == 10u &&
                fst[2].parentOrPosition == 1u &&
                fst[2].nextEntryOrLength == 6u &&
                fst[3].parentOrPosition == 2u &&
                fst[3].nextEntryOrLength == 5u &&
                fst[4].nextEntryOrLength == deepExpected.size() &&
                fst[5].nextEntryOrLength == leafExpected.size(),
            "host FST nodes did not use native SDK directory/file fields") ||
        !require(
            DVDConvertPathToEntrynum("/gxTests") == 1 &&
                DVDConvertPathToEntrynum("/gxTests/nested") == 2 &&
                DVDConvertPathToEntrynum(
                    "/gxTests/nested/deeper/deep.bin") == 4 &&
                DVDConvertPathToEntrynum("/gxTests/sample.bin") == 7 &&
                DVDConvertPathToEntrynum("/GXTESTS/SAMPLE.BIN") == 7,
            "host path lookup did not preserve deterministic FST entry IDs")) {
        fs::remove_all(root, error);
        return 1;
    }

    DVDFileInfo fastFile{};
    if (!require(
            DVDFastOpen(4, &fastFile) == TRUE,
            "DVDFastOpen rejected a recursive stable entry ID") ||
        !require(
            DVDGetLength(&fastFile) == deepExpected.size(),
            "DVDFastOpen did not use the matching FST file node") ||
        !require(
            DVDClose(&fastFile) == TRUE,
            "DVDClose rejected a recursively fast-opened file")) {
        fs::remove_all(root, error);
        return 1;
    }

    DVDDir fastDirectory{};
    DVDDirEntry fastEntry{};
    std::array<bool, 2> foundNestedChildren{};
    if (!require(
            DVDFastOpenDir(2, &fastDirectory) == TRUE,
            "DVDFastOpenDir rejected a recursive stable entry ID")) {
        fs::remove_all(root, error);
        return 1;
    }
    while (DVDReadDir(&fastDirectory, &fastEntry)) {
        if (fastEntry.entryNum == 3u && fastEntry.isDir &&
            std::string(fastEntry.name) == "deeper") {
            foundNestedChildren[0] = true;
        } else if (fastEntry.entryNum == 5u && !fastEntry.isDir &&
                   std::string(fastEntry.name) == "leaf.bin") {
            foundNestedChildren[1] = true;
        } else {
            DVDCloseDir(&fastDirectory);
            fs::remove_all(root, error);
            require(
                false,
                "recursive FST directory traversal leaked descendants");
            return 1;
        }
    }
    if (!require(
            foundNestedChildren[0] && foundNestedChildren[1],
            "recursive fast directory traversal missed a direct child") ||
        !require(
            DVDCloseDir(&fastDirectory) == TRUE,
            "DVDCloseDir rejected a recursively fast-opened directory")) {
        fs::remove_all(root, error);
        return 1;
    }

    TPLPalettePtr palette = nullptr;
    TPLGetPalette(&palette, "gxTests/valid.tpl");
    if (!require(
            palette != nullptr && palette->numDescriptors == 1u &&
                TPLGet(palette, 0) != nullptr &&
                TPLGet(palette, 0)->textureHeader != nullptr &&
                TPLGet(palette, 0)->textureHeader->width == 8u &&
                TPLGet(palette, 0)->textureHeader->height == 8u,
            "TPLGetPalette rejected a complete canonical big-endian TPL")) {
        TPLReleasePalette(&palette);
        fs::remove_all(root, error);
        return 1;
    }
    TPLReleasePalette(&palette);
    TPLGetPalette(&palette, "gxTests/truncated.tpl");
    if (!require(
            palette == nullptr,
            "TPLGetPalette accepted a truncated texture payload")) {
        TPLReleasePalette(&palette);
        fs::remove_all(root, error);
        return 1;
    }

    DVDFileInfo file{};
    if (!require(
            DVDOpen("gxTests/sample.bin", &file) == TRUE,
            "DVDOpen did not resolve a path beneath the host DVD root")) {
        fs::remove_all(root, error);
        return 1;
    }
    if (!require(
            DVDGetLength(&file) == expected.size(),
            "DVDOpen reported the wrong host-file length")) {
        DVDClose(&file);
        fs::remove_all(root, error);
        return 1;
    }

    std::array<std::uint8_t, 8> actual{};
    if (!require(
            DVDRead(&file, actual.data(), static_cast<s32>(actual.size()), 0) ==
                static_cast<s32>(actual.size()),
            "DVDRead did not return the complete host file") ||
        !require(actual == expected, "DVDRead returned incorrect bytes") ||
        !require(
            DVDGetTransferredSize(&file) ==
                static_cast<s32>(actual.size()),
            "DVDGetTransferredSize did not track the host read") ||
        !require(DVDClose(&file) == TRUE, "DVDClose rejected an open file")) {
        fs::remove_all(root, error);
        return 1;
    }

    if (!require(
            DVDOpen("gxTests/padded-read.bin", &file) == TRUE,
            "DVDOpen did not open the padded-read fixture")) {
        fs::remove_all(root, error);
        return 1;
    }
    alignas(32) std::array<std::uint8_t, 128> paddedActual{};
    std::fill(paddedActual.begin(), paddedActual.end(), 0xcd);
    if (!require(
            DVDRead(
                &file,
                paddedActual.data(),
                static_cast<s32>(paddedActual.size()),
                0) == static_cast<s32>(paddedActual.size()),
            "DVDRead did not complete a permitted aligned EOF read") ||
        !require(
            std::equal(
                paddedExpected.begin(),
                paddedExpected.end(),
                paddedActual.begin()),
            "DVDRead returned incorrect bytes before the logical EOF") ||
        !require(
            std::all_of(
                paddedActual.begin() + paddedExpected.size(),
                paddedActual.end(),
                [](std::uint8_t value) { return value == 0; }),
            "DVDRead did not zero-fill the permitted EOF padding") ||
        !require(
            DVDGetTransferredSize(&file) ==
                static_cast<s32>(paddedActual.size()),
            "DVDGetTransferredSize omitted permitted EOF padding") ||
        !require(
            DVDRead(
                &file,
                paddedActual.data(),
                static_cast<s32>(paddedActual.size()),
                24) == DVD_RESULT_FATAL_ERROR,
            "DVDRead accepted an end offset equal to file length plus 32") ||
        !require(DVDClose(&file) == TRUE, "DVDClose failed after padded read")) {
        fs::remove_all(root, error);
        return 1;
    }

    DVDDir directory{};
    DVDDirEntry entry{};
    bool foundFile = false;
    if (!require(
            DVDOpenDir("/gxTests", &directory) == TRUE,
            "DVDOpenDir did not open a host directory")) {
        fs::remove_all(root, error);
        return 1;
    }
    while (DVDReadDir(&directory, &entry)) {
        if (!entry.isDir && entry.entryNum == 7u &&
            std::string(entry.name) == "sample.bin") {
            foundFile = true;
        }
    }
    if (!require(foundFile, "DVDReadDir did not enumerate the host file")) {
        DVDCloseDir(&directory);
        fs::remove_all(root, error);
        return 1;
    }
    if (!require(
            DVDConvertPathToEntrynum("/gxTests/sample.bin") == 7,
            "directory enumeration changed a stable FST entry ID")) {
        DVDCloseDir(&directory);
        fs::remove_all(root, error);
        return 1;
    }
    DVDRewindDir(&directory);
    if (!require(
            DVDReadDir(&directory, &entry) == TRUE,
            "DVDRewindDir did not reset host enumeration") ||
        !require(
            DVDCloseDir(&directory) == TRUE,
            "DVDCloseDir rejected an open directory")) {
        fs::remove_all(root, error);
        return 1;
    }

    if (!require(
            DVDChangeDir("gxTests") == TRUE,
            "DVDChangeDir did not enter a host directory") ||
        !require(
            DVDOpen("sample.bin", &file) == TRUE,
            "DVDOpen did not honor the host current directory") ||
        !require(DVDClose(&file) == TRUE, "DVDClose failed after a relative open")) {
        fs::remove_all(root, error);
        return 1;
    }

    fs::remove_all(root, error);
    return 0;
}

extern "C" void DolphinMain() {
    std::exit(runHostFilesystemTest());
}
