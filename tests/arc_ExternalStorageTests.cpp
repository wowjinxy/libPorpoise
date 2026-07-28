#include <revolution/arc.h>

#include "sdk_ArchiveFixture.hpp"

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

bool ReadThroughArchiveOffsets(
    ARCHandle& handle,
    const std::vector<u8>& externalStorage,
    const char* path,
    const char* expected) {
    ARCFileInfo file{};
    if (!Require(
            ARCOpen(&handle, path, &file) == TRUE,
            "ARCOpen failed for metadata-only archive")) {
        return false;
    }

    const std::size_t offset = ARCGetStartOffset(&file);
    const std::size_t length = ARCGetLength(&file);
    return
        Require(
            offset + length <= externalStorage.size(),
            "ARC metadata points outside external storage") &&
        Require(
            length == std::strlen(expected),
            "ARC metadata reported the wrong external-file length") &&
        Require(
            std::memcmp(
                externalStorage.data() + offset,
                expected,
                length) == 0,
            "external read through ARC offsets returned the wrong bytes") &&
        Require(ARCClose(&file) == TRUE, "ARCClose failed");
}

bool TestMetadataOnlyArchive() {
    const std::vector<u8> externalStorage = BuildArchive();
    std::vector<u8> metadata(
        externalStorage.begin(),
        externalStorage.begin() + kFileStart);
    ARCHandle handle{};

    return
        Require(
            ARCInitHandle(metadata.data(), &handle) == TRUE,
            "ARCInitHandle rejected archive metadata without file payloads") &&
        ReadThroughArchiveOffsets(
            handle,
            externalStorage,
            "/assets/config.ini",
            "value\n") &&
        Require(
            ARCChangeDir(&handle, "/assets/nested") == TRUE,
            "ARCChangeDir failed in a metadata-only archive") &&
        ReadThroughArchiveOffsets(
            handle,
            externalStorage,
            "message.txt",
            "nested\n") &&
        ReadThroughArchiveOffsets(
            handle,
            externalStorage,
            "../../readme.txt",
            "root\n");
}

}  // namespace

int main() {
    return TestMetadataOnlyArchive() ? 0 : 1;
}
