#include <dolphin/dvd.h>
#include <dolphin/os.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

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
    const std::array<std::uint8_t, 8> expected{
        0x47, 0x58, 0x20, 0x44, 0x56, 0x44, 0x00, 0xff};

    std::error_code error;
    fs::create_directories(testDirectory, error);
    if (!require(!error, "failed to create temporary DVD root")) return 1;

    {
        std::ofstream output(testFile, std::ios::binary);
        output.write(
            reinterpret_cast<const char*>(expected.data()),
            static_cast<std::streamsize>(expected.size()));
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
        if (!entry.isDir && std::string(entry.name) == "sample.bin") {
            foundFile = true;
        }
    }
    if (!require(foundFile, "DVDReadDir did not enumerate the host file")) {
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
