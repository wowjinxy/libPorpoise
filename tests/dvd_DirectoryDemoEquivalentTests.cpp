#include "sdk_DvdFixture.hpp"

#include <dolphin/dvd.h>

#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

bool Check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

std::string JoinCurrentPath(const char* name)
{
    char current[1024]{};
    if (!DVDGetCurrentDir(current, sizeof(current))) {
        return {};
    }
    return std::string(current) == "/"
        ? std::string("/") + name
        : std::string(current) + "/" + name;
}

bool WalkDirectory(
    std::set<std::string>& directories,
    std::map<std::string, u32>& files,
    unsigned depth = 0
)
{
    DVDDir directory{};
    DVDDirEntry entry{};
    std::vector<DVDDirEntry> deferredDirectories;

    if (!Check(depth <= 16, "directory traversal exceeded maximum depth") ||
        !Check(DVDOpenDir(".", &directory), "DVDOpenDir(.) failed")) {
        return false;
    }

    unsigned entryCount = 0;
    while (DVDReadDir(&directory, &entry)) {
        if (!Check(++entryCount <= 32, "directory enumeration did not terminate")) {
            return false;
        }
        const std::string fullPath = JoinCurrentPath(entry.name);
        if (!Check(!fullPath.empty(), "DVDGetCurrentDir failed")) {
            return false;
        }

        if (entry.isDir) {
            directories.insert(fullPath);
            deferredDirectories.push_back(entry);
            continue;
        }

        DVDFileInfo file{};
        if (!Check(DVDOpen(entry.name, &file), "relative DVDOpen failed")) {
            return false;
        }
        files.emplace(fullPath, DVDGetLength(&file));

        DVDFileInfo fastFile{};
        if (!Check(
                DVDFastOpen(static_cast<s32>(entry.entryNum), &fastFile),
                "DVDFastOpen failed"
            ) ||
            !Check(
                DVDGetLength(&fastFile) == DVDGetLength(&file),
                "fast-open length differs"
            ) ||
            !Check(DVDClose(&fastFile), "fast-open DVDClose failed") ||
            !Check(DVDClose(&file), "DVDClose failed")) {
            return false;
        }
    }

    if (!Check(DVDCloseDir(&directory), "DVDCloseDir failed")) {
        return false;
    }

    for (const DVDDirEntry& deferred : deferredDirectories) {
        if (!Check(
                DVDChangeDir(deferred.name),
                "deferred DVDChangeDir failed; directory-entry name was not stable"
            ) ||
            !WalkDirectory(directories, files, depth + 1) ||
            !Check(DVDChangeDir(".."), "DVDChangeDir(..) failed")) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main()
{
    DvdFixture fixture("sdk-029-directory");
    const std::vector<std::uint8_t> rootData{1, 2, 3};
    const std::vector<std::uint8_t> logoData{4, 5, 6, 7};
    const std::vector<std::uint8_t> shipData{8, 9, 10, 11, 12};
    const std::vector<std::uint8_t> readmeData{'r', 'e', 'a', 'd', '\n'};

    if (!Check(fixture.valid(), "temporary DVD root creation failed") ||
        !Check(fixture.write("root.dat", rootData), "root.dat creation failed") ||
        !Check(
            fixture.write("assets/logo.bin", logoData),
            "logo.bin creation failed"
        ) ||
        !Check(
            fixture.write("assets/models/ship.bin", shipData),
            "ship.bin creation failed"
        ) ||
        !Check(
            fixture.write("texts/readme.txt", readmeData),
            "readme.txt creation failed"
        ) ||
        !Check(fixture.initialize(), "DVD host initialization failed")) {
        return 1;
    }

    std::set<std::string> directories;
    std::map<std::string, u32> files;
    if (!WalkDirectory(directories, files)) {
        return 1;
    }

    const std::set<std::string> expectedDirectories{
        "/assets",
        "/assets/models",
        "/texts",
    };
    const std::map<std::string, u32> expectedFiles{
        {"/assets/logo.bin", static_cast<u32>(logoData.size())},
        {"/assets/models/ship.bin", static_cast<u32>(shipData.size())},
        {"/root.dat", static_cast<u32>(rootData.size())},
        {"/texts/readme.txt", static_cast<u32>(readmeData.size())},
    };
    if (!Check(
            directories == expectedDirectories,
            "recursive directory set differs"
        ) ||
        !Check(files == expectedFiles, "recursive file listing differs")) {
        return 1;
    }

    char current[16]{};
    if (!Check(
            DVDGetCurrentDir(current, sizeof(current)) &&
                std::string(current) == "/",
            "directory traversal did not return to root"
        ) ||
        !Check(
            DVDConvertPathToEntrynum("/missing.bin") == -1,
            "missing path unexpectedly resolved"
        )) {
        return 1;
    }

    std::cout << "DVD recursive directory traversal passed\n";
    return 0;
}
