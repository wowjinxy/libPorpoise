#include <revolution/arc.h>

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    ARC_HEADER_SIZE = 32,
    ARC_NODE_SIZE = 12,
    ARC_NODE_DIRECTORY_FLAG = 0x01000000,
    ARC_NODE_NAME_MASK = 0x00ffffff
};

static u32 ARCReadBigEndian32(const void* address) {
    const u8* bytes = (const u8*)address;
    return ((u32)bytes[0] << 24) |
           ((u32)bytes[1] << 16) |
           ((u32)bytes[2] << 8) |
           (u32)bytes[3];
}

static const u8* ARCNodeAddress(const ARCHandle* handle, u32 entryNumber) {
    return (const u8*)handle->FSTStart + entryNumber * ARC_NODE_SIZE;
}

static u32 ARCNodeWord(
    const ARCHandle* handle,
    u32 entryNumber,
    u32 wordIndex) {
    return ARCReadBigEndian32(
        ARCNodeAddress(handle, entryNumber) + wordIndex * sizeof(u32));
}

static BOOL ARCNodeIsDirectory(
    const ARCHandle* handle,
    u32 entryNumber) {
    return (ARCNodeWord(handle, entryNumber, 0) &
            ARC_NODE_DIRECTORY_FLAG) != 0;
}

static char* ARCNodeName(
    const ARCHandle* handle,
    u32 entryNumber) {
    const u32 offset =
        ARCNodeWord(handle, entryNumber, 0) & ARC_NODE_NAME_MASK;
    return handle->FSTStringStart + offset;
}

static BOOL ARCNameMatches(
    const char* archiveName,
    const char* pathName,
    size_t pathLength) {
    size_t index;

    if (strlen(archiveName) != pathLength) {
        return FALSE;
    }

    for (index = 0; index < pathLength; ++index) {
        const unsigned char left = (unsigned char)archiveName[index];
        const unsigned char right = (unsigned char)pathName[index];
        if (tolower(left) != tolower(right)) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL ARCFindChild(
    ARCHandle* handle,
    u32 parent,
    const char* name,
    size_t nameLength,
    BOOL requireDirectory,
    u32* result) {
    const u32 end = ARCNodeWord(handle, parent, 2);
    u32 entry = parent + 1;

    while (entry < end) {
        const BOOL isDirectory = ARCNodeIsDirectory(handle, entry);
        const char* entryName = ARCNodeName(handle, entry);

        if (!(entryName[0] == '.' && entryName[1] == '\0') &&
            (!requireDirectory || isDirectory) &&
            ARCNameMatches(entryName, name, nameLength)) {
            *result = entry;
            return TRUE;
        }

        entry = isDirectory ? ARCNodeWord(handle, entry, 2) : entry + 1;
    }
    return FALSE;
}

static BOOL ARCValidateArchive(
    const u8* archive,
    u32 fstStart,
    u32 fstSize,
    u32 fileStart,
    u32* entryCount) {
    const u8* fst;
    u32 count;
    u32 stringTableOffset;
    u32 entry;

    if (fstStart < ARC_HEADER_SIZE ||
        fstSize < ARC_NODE_SIZE ||
        fileStart < fstStart ||
        fstSize > fileStart - fstStart) {
        return FALSE;
    }

    fst = archive + fstStart;
    if ((ARCReadBigEndian32(fst) & ARC_NODE_DIRECTORY_FLAG) == 0) {
        return FALSE;
    }

    count = ARCReadBigEndian32(fst + 8);
    if (count == 0 || count > fstSize / ARC_NODE_SIZE) {
        return FALSE;
    }

    stringTableOffset = count * ARC_NODE_SIZE;
    if (stringTableOffset >= fstSize) {
        return FALSE;
    }

    for (entry = 0; entry < count; ++entry) {
        const u8* node = fst + entry * ARC_NODE_SIZE;
        const u32 typeAndName = ARCReadBigEndian32(node);
        const u32 nameOffset = typeAndName & ARC_NODE_NAME_MASK;
        const BOOL isDirectory =
            (typeAndName & ARC_NODE_DIRECTORY_FLAG) != 0;
        const char* name;
        size_t maximumNameLength;

        if (nameOffset >= fstSize - stringTableOffset) {
            return FALSE;
        }
        name = (const char*)fst + stringTableOffset + nameOffset;
        maximumNameLength = fstSize - stringTableOffset - nameOffset;
        if (memchr(name, '\0', maximumNameLength) == NULL) {
            return FALSE;
        }

        if (isDirectory) {
            const u32 parent = ARCReadBigEndian32(node + 4);
            const u32 next = ARCReadBigEndian32(node + 8);
            if ((entry == 0 && (parent != 0 || next != count)) ||
                (entry != 0 &&
                 (parent >= count || next <= entry || next > count))) {
                return FALSE;
            }
        } else {
            const u32 offset = ARCReadBigEndian32(node + 4);
            if (offset < fileStart) {
                return FALSE;
            }
        }
    }

    *entryCount = count;
    return TRUE;
}

BOOL ARCInitHandle(void* archiveStart, ARCHandle* handle) {
    const u8* archive = (const u8*)archiveStart;
    u32 fstStart;
    u32 fstSize;
    u32 fileStart;
    u32 entryCount;

    if (archive == NULL || handle == NULL) {
        return FALSE;
    }
    memset(handle, 0, sizeof(*handle));

    if (ARCReadBigEndian32(archive) != DARCH_MAGIC) {
        return FALSE;
    }

    fstStart = ARCReadBigEndian32(archive + 4);
    fstSize = ARCReadBigEndian32(archive + 8);
    fileStart = ARCReadBigEndian32(archive + 12);
    if (!ARCValidateArchive(
            archive,
            fstStart,
            fstSize,
            fileStart,
            &entryCount)) {
        return FALSE;
    }

    handle->archiveStartAddr = archiveStart;
    handle->FSTStart = (void*)(archive + fstStart);
    handle->fileStart = (void*)(archive + fileStart);
    handle->entryNum = entryCount;
    handle->FSTStringStart =
        (char*)handle->FSTStart + entryCount * ARC_NODE_SIZE;
    handle->FSTLength = fstSize;
    handle->currDir = 0;
    return TRUE;
}

s32 ARCConvertPathToEntrynum(ARCHandle* handle, const char* path) {
    const char* cursor;
    u32 current;

    if (handle == NULL || handle->FSTStart == NULL || path == NULL) {
        return -1;
    }

    cursor = path;
    current = handle->currDir;
    if (*cursor == '/') {
        current = 0;
    }

    while (*cursor == '/') {
        ++cursor;
    }

    while (*cursor != '\0') {
        const char* segment = cursor;
        const char* afterSegment;
        size_t segmentLength;
        BOOL hadSeparator;
        BOOL requireDirectory;
        u32 child;

        while (*cursor != '\0' && *cursor != '/') {
            ++cursor;
        }
        afterSegment = cursor;
        segmentLength = (size_t)(afterSegment - segment);
        hadSeparator = *cursor == '/';
        while (*cursor == '/') {
            ++cursor;
        }
        requireDirectory = hadSeparator || *cursor != '\0';

        if (segmentLength == 1 && segment[0] == '.') {
            continue;
        }
        if (segmentLength == 2 &&
            segment[0] == '.' &&
            segment[1] == '.') {
            current = ARCNodeWord(handle, current, 1);
            continue;
        }

        if (!ARCFindChild(
                handle,
                current,
                segment,
                segmentLength,
                requireDirectory,
                &child)) {
            return -1;
        }
        current = child;
    }

    return (s32)current;
}

BOOL ARCEntrynumIsDir(const ARCHandle* handle, s32 entryNumber) {
    if (handle == NULL ||
        handle->FSTStart == NULL ||
        entryNumber < 0 ||
        (u32)entryNumber >= handle->entryNum) {
        return FALSE;
    }
    return ARCNodeIsDirectory(handle, (u32)entryNumber);
}

BOOL ARCOpen(
    ARCHandle* handle,
    const char* fileName,
    ARCFileInfo* fileInfo) {
    s32 entry;

    if (handle == NULL || fileName == NULL || fileInfo == NULL) {
        return FALSE;
    }
    entry = ARCConvertPathToEntrynum(handle, fileName);
    return ARCFastOpen(handle, entry, fileInfo);
}

BOOL ARCFastOpen(
    ARCHandle* handle,
    s32 entryNumber,
    ARCFileInfo* fileInfo) {
    if (handle == NULL ||
        fileInfo == NULL ||
        entryNumber < 0 ||
        (u32)entryNumber >= handle->entryNum ||
        ARCNodeIsDirectory(handle, (u32)entryNumber)) {
        return FALSE;
    }

    fileInfo->handle = handle;
    fileInfo->startOffset =
        ARCNodeWord(handle, (u32)entryNumber, 1);
    fileInfo->length =
        ARCNodeWord(handle, (u32)entryNumber, 2);
    return TRUE;
}

BOOL ARCGetCurrentDir(ARCHandle* handle, char* path, u32 maxLength) {
    u32* chain;
    u32 count = 0;
    u32 entry;
    size_t required = 1;
    size_t written = 0;
    BOOL complete = TRUE;

    if (handle == NULL ||
        handle->FSTStart == NULL ||
        path == NULL ||
        maxLength == 0 ||
        handle->currDir >= handle->entryNum ||
        !ARCNodeIsDirectory(handle, handle->currDir)) {
        return FALSE;
    }

    chain = (u32*)malloc(handle->entryNum * sizeof(*chain));
    if (chain == NULL) {
        path[0] = '\0';
        return FALSE;
    }

    entry = handle->currDir;
    while (entry != 0 && count < handle->entryNum) {
        const char* name = ARCNodeName(handle, entry);
        chain[count++] = entry;
        required += strlen(name) + 1;
        entry = ARCNodeWord(handle, entry, 1);
    }
    if (entry != 0) {
        free(chain);
        path[0] = '\0';
        return FALSE;
    }

#define ARC_APPEND_CHARACTER(character)                \
    do {                                                \
        if (written + 1 < maxLength) {                  \
            path[written] = (character);                \
        } else {                                        \
            complete = FALSE;                           \
        }                                               \
        ++written;                                      \
    } while (0)

    ARC_APPEND_CHARACTER('/');
    while (count > 0) {
        const char* name = ARCNodeName(handle, chain[--count]);
        while (*name != '\0') {
            ARC_APPEND_CHARACTER(*name++);
        }
        ARC_APPEND_CHARACTER('/');
    }

#undef ARC_APPEND_CHARACTER

    path[written < maxLength ? written : maxLength - 1] = '\0';
    if (required + 1 > maxLength) {
        complete = FALSE;
    }
    free(chain);
    return complete;
}

void* ARCGetStartAddrInMem(ARCFileInfo* fileInfo) {
    if (fileInfo == NULL ||
        fileInfo->handle == NULL ||
        fileInfo->handle->archiveStartAddr == NULL) {
        return NULL;
    }
    return (u8*)fileInfo->handle->archiveStartAddr +
           fileInfo->startOffset;
}

u32 ARCGetStartOffset(ARCFileInfo* fileInfo) {
    return fileInfo != NULL ? fileInfo->startOffset : 0;
}

u32 ARCGetLength(ARCFileInfo* fileInfo) {
    return fileInfo != NULL ? fileInfo->length : 0;
}

BOOL ARCClose(ARCFileInfo* fileInfo) {
    return fileInfo != NULL;
}

BOOL ARCChangeDir(ARCHandle* handle, const char* directoryName) {
    s32 entry;

    if (handle == NULL || directoryName == NULL) {
        return FALSE;
    }
    entry = ARCConvertPathToEntrynum(handle, directoryName);
    if (!ARCEntrynumIsDir(handle, entry)) {
        return FALSE;
    }
    handle->currDir = (u32)entry;
    return TRUE;
}

BOOL ARCOpenDir(
    ARCHandle* handle,
    const char* directoryName,
    ARCDir* directory) {
    s32 entry;

    if (handle == NULL ||
        directoryName == NULL ||
        directory == NULL) {
        return FALSE;
    }
    entry = ARCConvertPathToEntrynum(handle, directoryName);
    if (!ARCEntrynumIsDir(handle, entry)) {
        return FALSE;
    }

    directory->handle = handle;
    directory->entryNum = (u32)entry;
    directory->location = (u32)entry + 1;
    directory->next = ARCNodeWord(handle, (u32)entry, 2);
    return TRUE;
}

BOOL ARCReadDir(ARCDir* directory, ARCDirEntry* entry) {
    ARCHandle* handle;

    if (directory == NULL || entry == NULL) {
        return FALSE;
    }
    handle = directory->handle;
    if (handle == NULL || handle->FSTStart == NULL) {
        return FALSE;
    }

    while (directory->location > directory->entryNum &&
           directory->location < directory->next) {
        const u32 location = directory->location;
        const BOOL isDirectory =
            ARCNodeIsDirectory(handle, location);
        char* name = ARCNodeName(handle, location);

        directory->location = isDirectory
            ? ARCNodeWord(handle, location, 2)
            : location + 1;
        if (name[0] == '.' && name[1] == '\0') {
            continue;
        }

        entry->handle = handle;
        entry->entryNum = location;
        entry->isDir = isDirectory;
        entry->name = name;
        return TRUE;
    }
    return FALSE;
}

BOOL ARCCloseDir(ARCDir* directory) {
    return directory != NULL;
}
