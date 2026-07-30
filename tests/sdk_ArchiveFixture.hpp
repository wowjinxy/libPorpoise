#ifndef LIBPORPOISE_TESTS_SDK_ARCHIVE_FIXTURE_HPP
#define LIBPORPOISE_TESTS_SDK_ARCHIVE_FIXTURE_HPP

#include <revolution/arc.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace SdkArchiveFixture {

constexpr std::size_t kHeaderSize = 32;
constexpr std::size_t kNodeSize = 12;
constexpr std::size_t kNodeCount = 7;
constexpr std::size_t kFileStart = 192;

struct Node {
    bool isDirectory;
    std::string name;
    u32 value;
    u32 sizeOrNext;
};

inline void WriteBigEndian32(
    std::vector<u8>& bytes,
    std::size_t offset,
    u32 value) {
    bytes[offset] = static_cast<u8>(value >> 24);
    bytes[offset + 1] = static_cast<u8>(value >> 16);
    bytes[offset + 2] = static_cast<u8>(value >> 8);
    bytes[offset + 3] = static_cast<u8>(value);
}

inline std::vector<u8> BuildArchive() {
    constexpr std::array<u8, 6> configData{
        'v', 'a', 'l', 'u', 'e', '\n'};
    constexpr std::array<u8, 7> messageData{
        'n', 'e', 's', 't', 'e', 'd', '\n'};
    constexpr std::array<u8, 5> readmeData{
        'r', 'o', 'o', 't', '\n'};
    constexpr std::size_t configOffset = kFileStart;
    constexpr std::size_t messageOffset = configOffset + configData.size();
    constexpr std::size_t readmeOffset = messageOffset + messageData.size();

    const std::array<Node, kNodeCount> nodes{{
        {true, "", 0, 7},
        {true, "assets", 0, 5},
        {false, "config.ini", configOffset, configData.size()},
        {true, "nested", 1, 5},
        {false, "message.txt", messageOffset, messageData.size()},
        {false, "readme.txt", readmeOffset, readmeData.size()},
        {true, "empty", 0, 7},
    }};

    std::vector<u8> strings{0};
    std::array<u32, kNodeCount> nameOffsets{};
    for (std::size_t index = 1; index < nodes.size(); ++index) {
        nameOffsets[index] = static_cast<u32>(strings.size());
        strings.insert(
            strings.end(),
            nodes[index].name.begin(),
            nodes[index].name.end());
        strings.push_back(0);
    }

    const u32 fstSize =
        static_cast<u32>(kNodeCount * kNodeSize + strings.size());
    std::vector<u8> archive(readmeOffset + readmeData.size(), 0);
    WriteBigEndian32(archive, 0, DARCH_MAGIC);
    WriteBigEndian32(archive, 4, kHeaderSize);
    WriteBigEndian32(archive, 8, fstSize);
    WriteBigEndian32(archive, 12, kFileStart);

    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const std::size_t offset = kHeaderSize + index * kNodeSize;
        const u32 typeAndName =
            (nodes[index].isDirectory ? 0x01000000u : 0u) |
            nameOffsets[index];
        WriteBigEndian32(archive, offset, typeAndName);
        WriteBigEndian32(archive, offset + 4, nodes[index].value);
        WriteBigEndian32(archive, offset + 8, nodes[index].sizeOrNext);
    }

    std::copy(
        strings.begin(),
        strings.end(),
        archive.begin() + kHeaderSize + kNodeCount * kNodeSize);
    std::copy(
        configData.begin(),
        configData.end(),
        archive.begin() + configOffset);
    std::copy(
        messageData.begin(),
        messageData.end(),
        archive.begin() + messageOffset);
    std::copy(
        readmeData.begin(),
        readmeData.end(),
        archive.begin() + readmeOffset);
    return archive;
}

}  // namespace SdkArchiveFixture

#endif
