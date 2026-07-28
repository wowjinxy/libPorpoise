#include <revolution/cnt.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

struct ContentRegistration {
    u64 titleId;
    u32 contentIndex;
    fs::path root;
};

struct ContentEntry {
    fs::path relativePath;
    bool isDirectory;
};

struct HandleState {
    fs::path root;
    fs::path current;
    std::vector<ContentEntry> entries;
};

struct FileState {
    std::FILE* stream = nullptr;
};

struct DirectoryItem {
    std::string name;
    u32 entryNumber;
    bool isDirectory;
};

struct DirectoryState {
    std::vector<DirectoryItem> entries;
};

std::vector<ContentRegistration> Registrations;
bool Initialized;

bool IsWithinRoot(const fs::path& relative) {
    if (relative.empty()) {
        return true;
    }
    const auto first = *relative.begin();
    return first != ".." && !relative.is_absolute();
}

fs::path NormalizeRelativePath(
    const HandleState& state,
    const char* pathText) {
    if (pathText == nullptr) {
        return {};
    }

    std::string text(pathText);
    std::replace(text.begin(), text.end(), '\\', '/');
    bool absolute = !text.empty() && text.front() == '/';
    while (!text.empty() && text.front() == '/') {
        text.erase(text.begin());
    }

    fs::path relative =
        (absolute ? fs::path{} : state.current) / fs::path(text);
    relative = relative.lexically_normal();
    if (relative == ".") {
        relative.clear();
    }
    return relative;
}

bool ResolvePath(
    const HandleState& state,
    const char* pathText,
    fs::path& relative,
    fs::path& absolute) {
    relative = NormalizeRelativePath(state, pathText);
    if (!IsWithinRoot(relative)) {
        return false;
    }
    absolute = (state.root / relative).lexically_normal();
    return true;
}

void AppendEntries(
    HandleState& state,
    const fs::path& relativeDirectory) {
    std::vector<fs::directory_entry> children;
    std::error_code error;
    const fs::path absoluteDirectory = state.root / relativeDirectory;

    for (fs::directory_iterator iterator(absoluteDirectory, error), end;
         !error && iterator != end;
         iterator.increment(error)) {
        children.push_back(*iterator);
    }
    std::sort(
        children.begin(),
        children.end(),
        [](const fs::directory_entry& left, const fs::directory_entry& right) {
            return left.path().filename().string() <
                   right.path().filename().string();
        });

    for (const auto& child : children) {
        const fs::path relative = relativeDirectory / child.path().filename();
        const bool isDirectory = child.is_directory(error);
        state.entries.push_back({relative, isDirectory});
        if (isDirectory) {
            AppendEntries(state, relative);
        }
    }
}

void BuildEntryList(HandleState& state) {
    state.entries.clear();
    state.entries.push_back({{}, true});
    AppendEntries(state, {});
}

const ContentRegistration* FindRegistration(u64 titleId, u32 contentIndex) {
    for (const auto& registration : Registrations) {
        if (registration.titleId == titleId &&
            registration.contentIndex == contentIndex) {
            return &registration;
        }
    }
    return nullptr;
}

s32 InitHandle(
    u64 titleId,
    u32 contentIndex,
    CNTHandle* handle) {
    const ContentRegistration* registration;
    std::error_code error;
    auto state = std::make_unique<HandleState>();

    if (!Initialized || handle == nullptr) {
        return CNT_RESULT_BAD_STATUS;
    }
    registration = FindRegistration(titleId, contentIndex);
    if (registration == nullptr) {
        return CNT_RESULT_NOT_FOUND;
    }
    if (!fs::is_directory(registration->root, error) || error) {
        return CNT_RESULT_INVALID;
    }

    state->root = fs::weakly_canonical(registration->root, error);
    if (error) {
        return CNT_RESULT_INVALID;
    }
    state->current.clear();
    BuildEntryList(*state);

    std::memset(handle, 0, sizeof(*handle));
    handle->hostData = state.release();
    handle->titleId = titleId;
    handle->contentIndex = contentIndex;
    handle->type = CNT_TYPE_HOST;
    return CNT_RESULT_OK;
}

HandleState* GetHandle(CNTHandle* handle) {
    if (handle == nullptr ||
        handle->type != CNT_TYPE_HOST ||
        handle->hostData == nullptr) {
        return nullptr;
    }
    return static_cast<HandleState*>(handle->hostData);
}

const HandleState* GetHandle(const CNTHandle* handle) {
    return GetHandle(const_cast<CNTHandle*>(handle));
}

FileState* GetFile(CNTFileInfo* file) {
    if (file == nullptr || file->hostData == nullptr) {
        return nullptr;
    }
    return static_cast<FileState*>(file->hostData);
}

s32 OpenRelative(
    CNTHandle* handle,
    const fs::path& relative,
    CNTFileInfo* file) {
    HandleState* state = GetHandle(handle);
    std::error_code error;
    fs::path absolute;
    std::uintmax_t length;
    auto fileState = std::make_unique<FileState>();

    if (state == nullptr || file == nullptr || !IsWithinRoot(relative)) {
        return CNT_RESULT_INVALID;
    }
    absolute = (state->root / relative).lexically_normal();
    if (!fs::is_regular_file(absolute, error) || error) {
        return CNT_RESULT_NOT_FOUND;
    }
    length = fs::file_size(absolute, error);
    if (error || length > std::numeric_limits<u32>::max()) {
        return CNT_RESULT_IO_ERROR;
    }
    fileState->stream = std::fopen(absolute.string().c_str(), "rb");
    if (fileState->stream == nullptr) {
        return CNT_RESULT_ACCESS;
    }

    std::memset(file, 0, sizeof(*file));
    file->handle = handle;
    file->hostData = fileState.release();
    file->length = static_cast<u32>(length);
    file->position = 0;
    return CNT_RESULT_OK;
}

}  // namespace

extern "C" {

void CNTInit(void) {
    Initialized = true;
}

void CNTShutdown(void) {
    Initialized = false;
}

s32 CNTInitHandle(
    u32 contentIndex,
    CNTHandle* handle,
    MEMAllocator* allocator) {
    (void)allocator;
    return InitHandle(0, contentIndex, handle);
}

s32 CNTInitHandleTitle(
    u64 titleId,
    u32 contentIndex,
    CNTHandle* handle,
    MEMAllocator* allocator) {
    (void)allocator;
    return InitHandle(titleId, contentIndex, handle);
}

s32 CNTReleaseHandle(CNTHandle* handle) {
    HandleState* state = GetHandle(handle);
    if (state == nullptr) {
        return CNT_RESULT_BAD_STATUS;
    }
    delete state;
    std::memset(handle, 0, sizeof(*handle));
    return CNT_RESULT_OK;
}

s32 CNTOpen(
    CNTHandle* handle,
    const char* fileName,
    CNTFileInfo* file) {
    HandleState* state = GetHandle(handle);
    fs::path relative;
    fs::path absolute;

    if (state == nullptr ||
        !ResolvePath(*state, fileName, relative, absolute)) {
        return CNT_RESULT_INVALID;
    }
    return OpenRelative(handle, relative, file);
}

s32 CNTFastOpen(
    CNTHandle* handle,
    s32 entryNumber,
    CNTFileInfo* file) {
    HandleState* state = GetHandle(handle);
    if (state == nullptr ||
        entryNumber < 0 ||
        static_cast<std::size_t>(entryNumber) >= state->entries.size() ||
        state->entries[entryNumber].isDirectory) {
        return CNT_RESULT_INVALID;
    }
    return OpenRelative(handle, state->entries[entryNumber].relativePath, file);
}

s32 CNTRead(CNTFileInfo* file, void* destination, u32 length) {
    FileState* state = GetFile(file);
    std::size_t read;

    if (state == nullptr ||
        state->stream == nullptr ||
        destination == nullptr) {
        return CNT_RESULT_BAD_STATUS;
    }
    read = std::fread(destination, 1, length, state->stream);
    file->position += static_cast<s32>(read);
    if (read < length && std::ferror(state->stream)) {
        return CNT_RESULT_IO_ERROR;
    }
    return static_cast<s32>(read);
}

s32 CNTReadWithOffset(
    CNTFileInfo* file,
    void* destination,
    u32 length,
    s32 offset) {
    const s32 original = CNTTell(file);
    s32 result;

    if (original < 0 ||
        CNTSeek(file, offset, CNT_SEEK_SET) != CNT_RESULT_OK) {
        return CNT_RESULT_BAD_STATUS;
    }
    result = CNTRead(file, destination, length);
    CNTSeek(file, original, CNT_SEEK_SET);
    return result;
}

s32 CNTSeek(CNTFileInfo* file, s32 offset, u32 origin) {
    FileState* state = GetFile(file);
    s64 base;
    s64 next;

    if (state == nullptr || state->stream == nullptr) {
        return CNT_RESULT_BAD_STATUS;
    }
    switch (origin) {
    case CNT_SEEK_SET:
        base = 0;
        break;
    case CNT_SEEK_CUR:
        base = file->position;
        break;
    case CNT_SEEK_END:
        base = file->length;
        break;
    default:
        return CNT_RESULT_INVALID;
    }
    next = base + offset;
    if (next < 0 || next > file->length) {
        return CNT_RESULT_INVALID;
    }
    if (std::fseek(state->stream, static_cast<long>(next), SEEK_SET) != 0) {
        return CNT_RESULT_IO_ERROR;
    }
    file->position = static_cast<s32>(next);
    return CNT_RESULT_OK;
}

s32 CNTTell(CNTFileInfo* file) {
    return GetFile(file) != nullptr
        ? file->position
        : CNT_RESULT_BAD_STATUS;
}

u32 CNTGetLength(CNTFileInfo* file) {
    return GetFile(file) != nullptr ? file->length : 0;
}

s32 CNTClose(CNTFileInfo* file) {
    FileState* state = GetFile(file);
    if (state == nullptr) {
        return CNT_RESULT_BAD_STATUS;
    }
    const int result =
        state->stream != nullptr ? std::fclose(state->stream) : 0;
    delete state;
    std::memset(file, 0, sizeof(*file));
    return result == 0 ? CNT_RESULT_OK : CNT_RESULT_IO_ERROR;
}

s32 CNTConvertPathToEntrynum(
    CNTHandle* handle,
    const char* fileName) {
    HandleState* state = GetHandle(handle);
    fs::path relative;
    fs::path absolute;

    if (state == nullptr ||
        !ResolvePath(*state, fileName, relative, absolute)) {
        return -1;
    }
    for (std::size_t index = 0; index < state->entries.size(); ++index) {
        if (state->entries[index].relativePath == relative) {
            return static_cast<s32>(index);
        }
    }
    return -1;
}

BOOL CNTEntrynumIsDir(CNTHandle* handle, s32 entryNumber) {
    HandleState* state = GetHandle(handle);
    if (state == nullptr ||
        entryNumber < 0 ||
        static_cast<std::size_t>(entryNumber) >= state->entries.size()) {
        return FALSE;
    }
    return state->entries[entryNumber].isDirectory ? TRUE : FALSE;
}

s32 CNTChangeDir(CNTHandle* handle, const char* directoryName) {
    HandleState* state = GetHandle(handle);
    fs::path relative;
    fs::path absolute;
    std::error_code error;

    if (state == nullptr ||
        !ResolvePath(*state, directoryName, relative, absolute) ||
        !fs::is_directory(absolute, error) ||
        error) {
        return CNT_RESULT_NOT_FOUND;
    }
    state->current = relative;
    return CNT_RESULT_OK;
}

s32 CNTGetCurrentDir(CNTHandle* handle, char* path, u32 maxLength) {
    HandleState* state = GetHandle(handle);
    std::string current;

    if (state == nullptr || path == nullptr || maxLength == 0) {
        return CNT_RESULT_INVALID;
    }
    current = "/" + state->current.generic_string();
    if (current.size() > 1 && current.back() != '/') {
        current.push_back('/');
    }
    if (current.size() + 1 > maxLength) {
        std::memcpy(path, current.data(), maxLength - 1);
        path[maxLength - 1] = '\0';
        return CNT_RESULT_INVALID;
    }
    std::memcpy(path, current.c_str(), current.size() + 1);
    return CNT_RESULT_OK;
}

BOOL CNTOpenDir(
    CNTHandle* handle,
    const char* directoryName,
    CNTDir* directory) {
    HandleState* state = GetHandle(handle);
    fs::path relative;
    fs::path absolute;
    std::error_code error;
    auto directoryState = std::make_unique<DirectoryState>();

    if (state == nullptr ||
        directory == nullptr ||
        !ResolvePath(*state, directoryName, relative, absolute) ||
        !fs::is_directory(absolute, error) ||
        error) {
        return FALSE;
    }

    for (fs::directory_iterator iterator(absolute, error), end;
         !error && iterator != end;
         iterator.increment(error)) {
        const fs::path itemRelative =
            relative / iterator->path().filename();
        const s32 entryNumber =
            CNTConvertPathToEntrynum(handle, itemRelative.generic_string().c_str());
        directoryState->entries.push_back({
            iterator->path().filename().string(),
            entryNumber >= 0 ? static_cast<u32>(entryNumber) : 0,
            iterator->is_directory(error),
        });
    }
    if (error) {
        return FALSE;
    }
    std::sort(
        directoryState->entries.begin(),
        directoryState->entries.end(),
        [](const DirectoryItem& left, const DirectoryItem& right) {
            return left.name < right.name;
        });

    std::memset(directory, 0, sizeof(*directory));
    directory->handle = handle;
    directory->hostData = directoryState.release();
    return TRUE;
}

BOOL CNTReadDir(CNTDir* directory, CNTDirEntry* entry) {
    auto* state = directory != nullptr
        ? static_cast<DirectoryState*>(directory->hostData)
        : nullptr;
    if (state == nullptr ||
        entry == nullptr ||
        directory->location >= state->entries.size()) {
        return FALSE;
    }
    const DirectoryItem& item = state->entries[directory->location++];
    entry->entryNum = item.entryNumber;
    entry->isDir = item.isDirectory ? TRUE : FALSE;
    entry->name = item.name.c_str();
    return TRUE;
}

BOOL CNTCloseDir(CNTDir* directory) {
    if (directory == nullptr || directory->hostData == nullptr) {
        return FALSE;
    }
    delete static_cast<DirectoryState*>(directory->hostData);
    std::memset(directory, 0, sizeof(*directory));
    return TRUE;
}

u32 CNTTellDir(CNTDir* directory) {
    return directory != nullptr ? directory->location : 0;
}

void CNTSeekDir(CNTDir* directory, u32 location) {
    auto* state = directory != nullptr
        ? static_cast<DirectoryState*>(directory->hostData)
        : nullptr;
    if (state != nullptr) {
        directory->location = std::min<u32>(
            location,
            static_cast<u32>(state->entries.size()));
    }
}

void CNTRewindDir(CNTDir* directory) {
    CNTSeekDir(directory, 0);
}

BOOL CNTHostRegisterContent(
    u64 titleId,
    u32 contentIndex,
    const char* rootDirectory) {
    std::error_code error;
    fs::path root;

    if (rootDirectory == nullptr) {
        return FALSE;
    }
    root = fs::weakly_canonical(fs::path(rootDirectory), error);
    if (error || !fs::is_directory(root, error) || error) {
        return FALSE;
    }
    for (auto& registration : Registrations) {
        if (registration.titleId == titleId &&
            registration.contentIndex == contentIndex) {
            registration.root = root;
            return TRUE;
        }
    }
    Registrations.push_back({titleId, contentIndex, root});
    return TRUE;
}

void CNTHostClearContentRegistry(void) {
    Registrations.clear();
}

}  // extern "C"
