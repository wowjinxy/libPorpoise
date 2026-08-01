/*
 * Host implementation of the Dolphin/Revolution GD display-list file format.
 *
 * The on-disk format is always a 20-byte header followed by arrays of
 * 8-byte (offset, length) descriptors.  SDK sources represented those
 * offsets with C pointers and consequently depended on a 32-bit host ABI.
 * Keep the wire format fixed-width and construct native GDGList objects
 * explicitly so the same files work on both 32- and 64-bit hosts.
 */

#include <revolution/gd.h>
#include <dolphin/os/OSHostEndian.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    GD_FILE_HEADER_SIZE = 20,
    GD_FILE_DESC_SIZE = 8,
    GD_FILE_ALIGNMENT = 32
};

static int GDWriteBE32(FILE* file, u32 value)
{
    const u8 bytes[4] = {
        (u8)(value >> 24),
        (u8)(value >> 16),
        (u8)(value >> 8),
        (u8)value
    };
    return fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes) ? 0 : -1;
}

static int GDWriteBytes(FILE* file, const void* data, size_t size)
{
    return size == 0 || fwrite(data, 1, size, file) == size ? 0 : -1;
}

static int GDAddU32(u32 left, u32 right, u32* result)
{
    if (left > UINT32_MAX - right) {
        return 0;
    }
    *result = left + right;
    return 1;
}

static int GDMulU32(u32 left, u32 right, u32* result)
{
    if (left != 0 && right > UINT32_MAX / left) {
        return 0;
    }
    *result = left * right;
    return 1;
}

static int GDRoundUp32(u32 value, u32* result)
{
    u32 withPadding;
    if (!GDAddU32(value, GD_FILE_ALIGNMENT - 1, &withPadding)) {
        return 0;
    }
    *result = withPadding & ~(u32)(GD_FILE_ALIGNMENT - 1);
    return 1;
}

static int GDRangeInFile(u32 offset, u32 length, size_t fileLength)
{
    return (size_t)offset <= fileLength &&
           (size_t)length <= fileLength - (size_t)offset;
}

static int GDWriteFailure(FILE* file)
{
    fclose(file);
    return -1;
}

s32 GDWriteDLFile(char* fName, u32 numDLs, u32 numPLs,
                  GDGList* DLDescArray, GDGList* PLDescArray)
{
    FILE* file;
    u32 dlTableBytes;
    u32 plTableBytes;
    u32 plTableOffset;
    u32 dataOffset;
    u32 unalignedDataOffset;
    u32 i;

    if (fName == NULL || (numDLs != 0 && DLDescArray == NULL) ||
        (numPLs != 0 && PLDescArray == NULL)) {
        return -3;
    }

    file = fopen(fName, "wb");
    if (file == NULL) {
        return -3;
    }

    if (!GDMulU32(numDLs, GD_FILE_DESC_SIZE, &dlTableBytes) ||
        !GDMulU32(numPLs, GD_FILE_DESC_SIZE, &plTableBytes) ||
        !GDAddU32(GD_FILE_HEADER_SIZE, dlTableBytes, &plTableOffset) ||
        !GDAddU32(plTableOffset, plTableBytes, &unalignedDataOffset) ||
        !GDRoundUp32(unalignedDataOffset, &dataOffset)) {
        return GDWriteFailure(file);
    }

    if (GDWriteBE32(file, GDFileVersionNumber) ||
        GDWriteBE32(file, numDLs) ||
        GDWriteBE32(file, numPLs) ||
        GDWriteBE32(file, GD_FILE_HEADER_SIZE) ||
        GDWriteBE32(file, plTableOffset)) {
        return GDWriteFailure(file);
    }

    for (i = 0; i < numDLs; ++i) {
        if ((DLDescArray[i].byteLength & 31) != 0 ||
            (DLDescArray[i].byteLength != 0 && DLDescArray[i].ptr == NULL) ||
            GDWriteBE32(file, dataOffset) ||
            GDWriteBE32(file, DLDescArray[i].byteLength) ||
            !GDAddU32(dataOffset, DLDescArray[i].byteLength, &dataOffset)) {
            return GDWriteFailure(file);
        }
    }

    for (i = 0; i < numPLs; ++i) {
        if ((PLDescArray[i].byteLength != 0 && PLDescArray[i].ptr == NULL) ||
            GDWriteBE32(file, dataOffset) ||
            GDWriteBE32(file, PLDescArray[i].byteLength) ||
            !GDAddU32(dataOffset, PLDescArray[i].byteLength, &dataOffset)) {
            return GDWriteFailure(file);
        }
    }

    {
        static const u8 zeroes[GD_FILE_ALIGNMENT] = {0};
        const u32 padding = dataOffset >= unalignedDataOffset
                                ? (GDRoundUp32(unalignedDataOffset, &dataOffset),
                                   dataOffset - unalignedDataOffset)
                                : 0;
        if (GDWriteBytes(file, zeroes, padding)) {
            return GDWriteFailure(file);
        }
    }

    for (i = 0; i < numDLs; ++i) {
        if (GDWriteBytes(file, DLDescArray[i].ptr,
                         DLDescArray[i].byteLength)) {
            return GDWriteFailure(file);
        }
    }

    for (i = 0; i < numPLs; ++i) {
        const u8* bytes = (const u8*)PLDescArray[i].ptr;
        u32 remaining = PLDescArray[i].byteLength;

        while (remaining >= sizeof(u32)) {
            u32 value;
            memcpy(&value, bytes, sizeof(value));
            if (GDWriteBE32(file, value)) {
                return GDWriteFailure(file);
            }
            bytes += sizeof(u32);
            remaining -= sizeof(u32);
        }
        if (GDWriteBytes(file, bytes, remaining)) {
            return GDWriteFailure(file);
        }
    }

    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}

s32 GDReadDLFile(const char* fName, u32* numDLs, u32* numPLs,
                 GDGList** DLDescArray, GDGList** PLDescArray)
{
    FILE* file;
    long lengthLong;
    size_t fileLength;
    u8* fileBytes = NULL;
    u8* allocation = NULL;
    u8* retainedFile;
    GDGList* dlLists;
    GDGList* plLists;
    u32 dlCount;
    u32 plCount;
    u32 dlOffset;
    u32 plOffset;
    u32 dlTableBytes;
    u32 plTableBytes;
    size_t dlHostBytes;
    size_t plHostBytes;
    size_t descriptorBytes;
    size_t allocationBytes;
    u32 i;

    if (numDLs == NULL || numPLs == NULL || DLDescArray == NULL ||
        PLDescArray == NULL) {
        return -2;
    }
    *numDLs = 0;
    *numPLs = 0;
    *DLDescArray = NULL;
    *PLDescArray = NULL;

    if (fName == NULL || (file = fopen(fName, "rb")) == NULL) {
        return -3;
    }
    if (fseek(file, 0, SEEK_END) != 0 ||
        (lengthLong = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -2;
    }
    fileLength = (size_t)lengthLong;
    if (fileLength < GD_FILE_HEADER_SIZE) {
        fclose(file);
        return -2;
    }

    fileBytes = (u8*)malloc(fileLength);
    if (fileBytes == NULL) {
        fclose(file);
        return -5;
    }
    if (fread(fileBytes, 1, fileLength, file) != fileLength) {
        fclose(file);
        free(fileBytes);
        return -2;
    }
    fclose(file);

    if (OSReadBigEndian32(fileBytes) != GDFileVersionNumber) {
        free(fileBytes);
        return -4;
    }
    dlCount = OSReadBigEndian32(fileBytes + 4);
    plCount = OSReadBigEndian32(fileBytes + 8);
    dlOffset = OSReadBigEndian32(fileBytes + 12);
    plOffset = OSReadBigEndian32(fileBytes + 16);

    if (!GDMulU32(dlCount, GD_FILE_DESC_SIZE, &dlTableBytes) ||
        !GDMulU32(plCount, GD_FILE_DESC_SIZE, &plTableBytes) ||
        (size_t)dlCount > SIZE_MAX / sizeof(GDGList) ||
        (size_t)plCount > SIZE_MAX / sizeof(GDGList)) {
        free(fileBytes);
        return -2;
    }
    dlHostBytes = (size_t)dlCount * sizeof(GDGList);
    plHostBytes = (size_t)plCount * sizeof(GDGList);
    if (dlHostBytes > SIZE_MAX - plHostBytes) {
        free(fileBytes);
        return -2;
    }
    descriptorBytes = dlHostBytes + plHostBytes;
    if (!GDRangeInFile(dlOffset, dlTableBytes, fileLength) ||
        !GDRangeInFile(plOffset, plTableBytes, fileLength) ||
        descriptorBytes > SIZE_MAX - (GD_FILE_ALIGNMENT - 1) ||
        descriptorBytes + (GD_FILE_ALIGNMENT - 1) > SIZE_MAX - fileLength) {
        free(fileBytes);
        return -2;
    }

    allocationBytes =
        descriptorBytes + (GD_FILE_ALIGNMENT - 1) + fileLength;
    allocation = (u8*)malloc(allocationBytes);
    if (allocation == NULL) {
        free(fileBytes);
        return -5;
    }

    dlLists = dlCount != 0 ? (GDGList*)allocation : NULL;
    plLists = plCount != 0
                  ? (GDGList*)(allocation + dlHostBytes)
                  : NULL;
    retainedFile = (u8*)(((uintptr_t)(allocation + descriptorBytes) +
                          (GD_FILE_ALIGNMENT - 1)) &
                         ~(uintptr_t)(GD_FILE_ALIGNMENT - 1));
    memcpy(retainedFile, fileBytes, fileLength);
    free(fileBytes);

    for (i = 0; i < dlCount; ++i) {
        const u8* desc = retainedFile + dlOffset + i * GD_FILE_DESC_SIZE;
        const u32 dataOffset = OSReadBigEndian32(desc);
        const u32 byteLength = OSReadBigEndian32(desc + 4);
        if (!GDRangeInFile(dataOffset, byteLength, fileLength)) {
            free(allocation);
            return -2;
        }
        dlLists[i].ptr = retainedFile + dataOffset;
        dlLists[i].byteLength = byteLength;
    }

    for (i = 0; i < plCount; ++i) {
        const u8* desc = retainedFile + plOffset + i * GD_FILE_DESC_SIZE;
        const u32 dataOffset = OSReadBigEndian32(desc);
        const u32 byteLength = OSReadBigEndian32(desc + 4);
        if (!GDRangeInFile(dataOffset, byteLength, fileLength)) {
            free(allocation);
            return -2;
        }
        plLists[i].ptr = retainedFile + dataOffset;
        plLists[i].byteLength = byteLength;
    }

    *numDLs = dlCount;
    *numPLs = plCount;
    *DLDescArray = dlLists;
    *PLDescArray = plLists;
    return 0;
}
