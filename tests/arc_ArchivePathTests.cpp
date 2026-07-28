#include <revolution/arc.h>

#include "sdk_ArchiveFixture.hpp"

#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace SdkArchiveFixture;

bool Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

bool CheckOpen(
    ARCHandle& handle,
    const char* path,
    const char* expected,
    std::size_t expectedLength) {
    ARCFileInfo file{};
    return
        Require(ARCOpen(&handle, path, &file) == TRUE, "ARCOpen failed") &&
        Require(
            ARCGetLength(&file) == expectedLength,
            "ARCGetLength returned the wrong size") &&
        Require(
            std::memcmp(
                ARCGetStartAddrInMem(&file),
                expected,
                expectedLength) == 0,
            "ARCGetStartAddrInMem returned the wrong file bytes") &&
        Require(ARCClose(&file) == TRUE, "ARCClose failed");
}

bool TestInMemoryArchivePaths() {
    std::vector<u8> archive = BuildArchive();
    ARCHandle handle{};
    char currentDirectory[64]{};

    if (!Require(
            ARCInitHandle(archive.data(), &handle) == TRUE,
            "ARCInitHandle rejected a valid archive") ||
        !CheckOpen(
            handle,
            "/assets/config.ini",
            "value\n",
            std::strlen("value\n")) ||
        !CheckOpen(
            handle,
            "ASSETS/CONFIG.INI",
            "value\n",
            std::strlen("value\n")) ||
        !Require(
            ARCChangeDir(&handle, "assets/nested/") == TRUE,
            "ARCChangeDir failed for a relative nested path") ||
        !Require(
            ARCGetCurrentDir(
                &handle,
                currentDirectory,
                sizeof(currentDirectory)) == TRUE,
            "ARCGetCurrentDir failed") ||
        !Require(
            std::string(currentDirectory) == "/assets/nested/",
            "ARCGetCurrentDir returned the wrong path") ||
        !CheckOpen(
            handle,
            "message.txt",
            "nested\n",
            std::strlen("nested\n")) ||
        !CheckOpen(
            handle,
            "../config.ini",
            "value\n",
            std::strlen("value\n")) ||
        !Require(
            ARCChangeDir(&handle, "/") == TRUE,
            "ARCChangeDir failed to return to root") ||
        !CheckOpen(
            handle,
            "../readme.txt",
            "root\n",
            std::strlen("root\n"))) {
        return false;
    }

    const s32 configEntry =
        ARCConvertPathToEntrynum(&handle, "assets/config.ini");
    ARCFileInfo configFile{};
    return
        Require(configEntry == 2, "path conversion returned the wrong entry") &&
        Require(
            ARCEntrynumIsDir(&handle, configEntry) == FALSE,
            "file entry was reported as a directory") &&
        Require(
            ARCFastOpen(&handle, configEntry, &configFile) == TRUE,
            "ARCFastOpen rejected a file entry") &&
        Require(
            ARCGetStartOffset(&configFile) == kFileStart,
            "ARCGetStartOffset returned the wrong archive offset") &&
        Require(
            ARCOpen(&handle, "assets", &configFile) == FALSE,
            "ARCOpen accepted a directory") &&
        Require(
            ARCChangeDir(&handle, "readme.txt") == FALSE,
            "ARCChangeDir accepted a file") &&
        Require(
            ARCOpen(&handle, "missing.bin", &configFile) == FALSE,
            "ARCOpen accepted a missing path");
}

bool TestInvalidArchiveRejected() {
    std::vector<u8> archive = BuildArchive();
    ARCHandle handle{};
    archive[0] = 0;
    return Require(
        ARCInitHandle(archive.data(), &handle) == FALSE,
        "ARCInitHandle accepted an invalid magic value");
}

}  // namespace

int main() {
    if (!TestInMemoryArchivePaths()) {
        return 1;
    }
    if (!TestInvalidArchiveRejected()) {
        return 2;
    }
    return 0;
}
