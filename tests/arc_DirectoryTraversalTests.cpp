#include <revolution/arc.h>

#include "sdk_ArchiveFixture.hpp"

#include <iostream>
#include <set>
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

bool WalkDirectory(
    ARCHandle& handle,
    const std::string& parentPath,
    std::set<std::string>& observed) {
    ARCDir directory{};
    ARCDirEntry entry{};
    std::vector<std::string> childDirectories;

    if (!Require(
            ARCOpenDir(&handle, ".", &directory) == TRUE,
            "ARCOpenDir failed during recursive traversal")) {
        return false;
    }

    while (ARCReadDir(&directory, &entry)) {
        const std::string path =
            parentPath + "/" + entry.name;
        if (entry.isDir) {
            observed.insert("D " + path);
            childDirectories.emplace_back(entry.name);
        } else {
            ARCFileInfo file{};
            if (!Require(
                    ARCOpen(&handle, entry.name, &file) == TRUE,
                    "ARCOpen failed for an enumerated file")) {
                return false;
            }
            observed.insert(
                "F " + path + ":" +
                std::to_string(ARCGetLength(&file)));
            ARCClose(&file);
        }
    }
    if (!Require(
            ARCCloseDir(&directory) == TRUE,
            "ARCCloseDir failed")) {
        return false;
    }

    for (const std::string& child : childDirectories) {
        if (!Require(
                ARCChangeDir(&handle, child.c_str()) == TRUE,
                "ARCChangeDir failed for an enumerated directory") ||
            !WalkDirectory(
                handle,
                parentPath + "/" + child,
                observed) ||
            !Require(
                ARCChangeDir(&handle, "..") == TRUE,
                "ARCChangeDir failed to leave an enumerated directory")) {
            return false;
        }
    }
    return true;
}

bool TestRecursiveDirectoryTraversal() {
    std::vector<u8> archive = BuildArchive();
    ARCHandle handle{};
    std::set<std::string> observed;
    const std::set<std::string> expected{
        "D /assets",
        "D /assets/nested",
        "D /empty",
        "F /assets/config.ini:6",
        "F /assets/nested/message.txt:7",
        "F /readme.txt:5",
    };

    return
        Require(
            ARCInitHandle(archive.data(), &handle) == TRUE,
            "ARCInitHandle rejected the traversal fixture") &&
        WalkDirectory(handle, "", observed) &&
        Require(
            observed == expected,
            "recursive ARC directory traversal returned the wrong tree");
}

}  // namespace

int main() {
    return TestRecursiveDirectoryTraversal() ? 0 : 1;
}
