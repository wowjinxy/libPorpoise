#include <revolution/darch.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr std::uint32_t kArchiveMagic = 0x55AA382D;
constexpr std::uint32_t kHeaderSize = 0x20;
constexpr std::uint32_t kNodeSize = 12;
constexpr std::uint32_t kDataAlignment = 32;

struct Node {
    std::string name;
    bool isDirectory = true;
    const void* data = nullptr;
    std::uint32_t length = 0;
    std::vector<std::unique_ptr<Node>> children;
    std::unordered_map<std::string, Node*> childByName;
};

struct FlatNode {
    const Node* node;
    std::uint32_t parent;
    std::uint32_t next;
    std::uint32_t nameOffset;
    std::uint32_t dataOffset;
};

struct ArchiveLayout {
    Node root;
    std::vector<FlatNode> nodes;
    std::string strings;
    std::uint32_t fstSize = 0;
    std::uint32_t dataStart = 0;
    std::uint32_t archiveSize = 0;
};

std::uint64_t AlignUp(std::uint64_t value)
{
    return (value + kDataAlignment - 1) &
           ~(static_cast<std::uint64_t>(kDataAlignment) - 1);
}

bool AddChecked(std::uint64_t& value, std::uint64_t addend)
{
    value += addend;
    return value <= std::numeric_limits<std::uint32_t>::max();
}

bool SplitPath(const char* pathName, std::vector<std::string>& parts)
{
    if (pathName == nullptr || *pathName == '\0') {
        return false;
    }

    std::string path(pathName);
    std::replace(path.begin(), path.end(), '\\', '/');
    if (path.front() == '/') {
        return false;
    }

    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t slash = path.find('/', start);
        const std::size_t end = slash == std::string::npos ? path.size() : slash;
        const std::string part = path.substr(start, end - start);
        if (part.empty() || part == "..") {
            return false;
        }
        if (part != ".") {
            parts.push_back(part);
        }
        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1;
    }
    return !parts.empty();
}

bool InsertFile(Node& root, const DARCHFileInfo& file)
{
    if (file.length != 0 && file.fileStart == nullptr) {
        return false;
    }

    std::vector<std::string> parts;
    if (!SplitPath(file.pathName, parts)) {
        return false;
    }

    Node* parent = &root;
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        const auto found = parent->childByName.find(parts[i]);
        if (found != parent->childByName.end()) {
            if (!found->second->isDirectory) {
                return false;
            }
            parent = found->second;
            continue;
        }

        auto directory = std::make_unique<Node>();
        directory->name = parts[i];
        Node* directoryPtr = directory.get();
        parent->children.push_back(std::move(directory));
        parent->childByName.emplace(parts[i], directoryPtr);
        parent = directoryPtr;
    }

    const std::string& fileName = parts.back();
    if (parent->childByName.find(fileName) != parent->childByName.end()) {
        return false;
    }

    auto newFile = std::make_unique<Node>();
    newFile->name = fileName;
    newFile->isDirectory = false;
    newFile->data = file.fileStart;
    newFile->length = file.length;
    Node* filePtr = newFile.get();
    parent->children.push_back(std::move(newFile));
    parent->childByName.emplace(fileName, filePtr);
    return true;
}

bool FlattenTree(
    const Node& node,
    std::uint32_t parent,
    ArchiveLayout& layout,
    std::uint32_t& nodeIndex
)
{
    if (layout.nodes.size() >= 0x01000000u || layout.strings.size() > 0x00FFFFFFu) {
        return false;
    }

    const std::uint32_t current = nodeIndex++;
    layout.nodes.push_back({
        &node,
        parent,
        0,
        static_cast<std::uint32_t>(layout.strings.size()),
        0,
    });
    layout.strings.append(node.name);
    layout.strings.push_back('\0');

    if (node.isDirectory) {
        for (const auto& child : node.children) {
            if (!FlattenTree(*child, current, layout, nodeIndex)) {
                return false;
            }
        }
        layout.nodes[current].next = nodeIndex;
    }
    return true;
}

bool BuildLayout(
    const DARCHFileInfo* fileInfo,
    std::uint32_t fileInfoNum,
    ArchiveLayout& layout
)
{
    if (fileInfo == nullptr || fileInfoNum == 0) {
        return false;
    }

    for (std::uint32_t i = 0; i < fileInfoNum; ++i) {
        if (!InsertFile(layout.root, fileInfo[i])) {
            return false;
        }
    }

    std::uint32_t nodeIndex = 0;
    if (!FlattenTree(layout.root, 0, layout, nodeIndex)) {
        return false;
    }

    std::uint64_t fstSize =
        static_cast<std::uint64_t>(layout.nodes.size()) * kNodeSize;
    if (!AddChecked(fstSize, layout.strings.size())) {
        return false;
    }
    layout.fstSize = static_cast<std::uint32_t>(fstSize);

    std::uint64_t dataStart = kHeaderSize;
    if (!AddChecked(dataStart, fstSize)) {
        return false;
    }
    dataStart = AlignUp(dataStart);
    if (dataStart > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    layout.dataStart = static_cast<std::uint32_t>(dataStart);

    std::vector<std::size_t> files;
    for (std::size_t i = 0; i < layout.nodes.size(); ++i) {
        if (!layout.nodes[i].node->isDirectory) {
            files.push_back(i);
        }
    }

    std::uint64_t cursor = dataStart;
    for (std::size_t i = 0; i < files.size(); ++i) {
        FlatNode& flat = layout.nodes[files[i]];
        flat.dataOffset = static_cast<std::uint32_t>(cursor);
        if (!AddChecked(cursor, flat.node->length)) {
            return false;
        }
        if (i + 1 != files.size()) {
            cursor = AlignUp(cursor);
            if (cursor > std::numeric_limits<std::uint32_t>::max()) {
                return false;
            }
        }
    }
    layout.archiveSize = static_cast<std::uint32_t>(cursor);
    return true;
}

void WriteBe32(std::uint8_t* dst, std::uint32_t value)
{
    dst[0] = static_cast<std::uint8_t>(value >> 24);
    dst[1] = static_cast<std::uint8_t>(value >> 16);
    dst[2] = static_cast<std::uint8_t>(value >> 8);
    dst[3] = static_cast<std::uint8_t>(value);
}

}  // namespace

extern "C" u32 DARCHGetArcSize(const DARCHFileInfo* fileInfo, u32 fileInfoNum)
{
    ArchiveLayout layout;
    return BuildLayout(fileInfo, fileInfoNum, layout) ? layout.archiveSize : 0;
}

extern "C" BOOL DARCHCreate(
    void* dst,
    u32 dstSize,
    const DARCHFileInfo* fileInfo,
    u32 fileInfoNum
)
{
    ArchiveLayout layout;
    if (dst == nullptr || !BuildLayout(fileInfo, fileInfoNum, layout) ||
        dstSize < layout.archiveSize) {
        return FALSE;
    }

    auto* archive = static_cast<std::uint8_t*>(dst);
    std::memset(archive, 0, layout.archiveSize);
    WriteBe32(archive + 0x00, kArchiveMagic);
    WriteBe32(archive + 0x04, kHeaderSize);
    WriteBe32(archive + 0x08, layout.fstSize);
    WriteBe32(archive + 0x0C, layout.dataStart);

    std::uint8_t* fst = archive + kHeaderSize;
    for (std::size_t i = 0; i < layout.nodes.size(); ++i) {
        const FlatNode& flat = layout.nodes[i];
        std::uint8_t* entry = fst + i * kNodeSize;
        WriteBe32(
            entry,
            (flat.node->isDirectory ? 0x01000000u : 0u) | flat.nameOffset
        );
        if (flat.node->isDirectory) {
            WriteBe32(entry + 4, flat.parent);
            WriteBe32(entry + 8, flat.next);
        } else {
            WriteBe32(entry + 4, flat.dataOffset);
            WriteBe32(entry + 8, flat.node->length);
            if (flat.node->length != 0) {
                std::memcpy(
                    archive + flat.dataOffset,
                    flat.node->data,
                    flat.node->length
                );
            }
        }
    }

    std::uint8_t* stringTable = fst + layout.nodes.size() * kNodeSize;
    std::memcpy(stringTable, layout.strings.data(), layout.strings.size());
    return TRUE;
}
