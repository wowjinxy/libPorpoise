#include <dolphin/os/OSHostEndian.h>
#include <revolution/gd.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

bool WriteBytes(const char* path, const std::vector<u8>& bytes)
{
    FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        return false;
    }
    const bool wrote =
        std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    const bool closed = std::fclose(file) == 0;
    return wrote && closed;
}

std::vector<u8> ReadBytes(const char* path)
{
    FILE* file = std::fopen(path, "rb");
    if (file == nullptr || std::fseek(file, 0, SEEK_END) != 0) {
        if (file != nullptr) {
            std::fclose(file);
        }
        return {};
    }
    const long length = std::ftell(file);
    if (length < 0 || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return {};
    }
    std::vector<u8> bytes(static_cast<std::size_t>(length));
    if ((!bytes.empty() &&
         std::fread(bytes.data(), 1, bytes.size(), file) != bytes.size()) ||
        std::fclose(file) != 0) {
        return {};
    }
    return bytes;
}

}

int main()
{
    const char* path = "gd_file_endian_test.bin";
    const char* invalidPath = "gd_file_invalid_count_test.bin";
    std::remove(path);
    std::remove(invalidPath);

    std::array<u8, 32> displayList = {};
    for (std::size_t index = 0; index < displayList.size(); ++index) {
        displayList[index] = static_cast<u8>(0x40u + index);
    }
    std::array<u32, 2> patches = {0x01020304u, 0xaabbccddu};
    GDGList dl = {displayList.data(), static_cast<u32>(displayList.size())};
    GDGList pl = {patches.data(), static_cast<u32>(sizeof(patches))};
    if (GDWriteDLFile(const_cast<char*>(path), 1, 1, &dl, &pl) != 0) {
        return 1;
    }

    const std::vector<u8> bytes = ReadBytes(path);
    if (bytes.size() != 104u ||
        OSReadBigEndian32(bytes.data() + 0) != GDFileVersionNumber ||
        OSReadBigEndian32(bytes.data() + 4) != 1u ||
        OSReadBigEndian32(bytes.data() + 8) != 1u ||
        OSReadBigEndian32(bytes.data() + 12) != 20u ||
        OSReadBigEndian32(bytes.data() + 16) != 28u ||
        OSReadBigEndian32(bytes.data() + 20) != 64u ||
        OSReadBigEndian32(bytes.data() + 24) != 32u ||
        OSReadBigEndian32(bytes.data() + 28) != 96u ||
        OSReadBigEndian32(bytes.data() + 32) != 8u ||
        std::memcmp(bytes.data() + 64, displayList.data(), 32) != 0 ||
        OSReadBigEndian32(bytes.data() + 96) != patches[0] ||
        OSReadBigEndian32(bytes.data() + 100) != patches[1]) {
        std::remove(path);
        return 2;
    }

    u32 dlCount = 0;
    u32 plCount = 0;
    GDGList* dlOut = nullptr;
    GDGList* plOut = nullptr;
    if (GDReadDLFile(path, &dlCount, &plCount, &dlOut, &plOut) != 0 ||
        dlCount != 1u || plCount != 1u || dlOut == nullptr ||
        plOut == nullptr || dlOut[0].byteLength != 32u ||
        plOut[0].byteLength != 8u ||
        std::memcmp(dlOut[0].ptr, displayList.data(), 32) != 0 ||
        OSReadBigEndian32(plOut[0].ptr) != patches[0] ||
        OSReadBigEndian32(static_cast<const u8*>(plOut[0].ptr) + 4) !=
            patches[1]) {
        std::free(dlOut);
        std::remove(path);
        return 3;
    }
    std::free(dlOut);
    std::remove(path);

    std::vector<u8> invalid(20, 0);
    OSWriteBigEndian32(invalid.data() + 0, GDFileVersionNumber);
    OSWriteBigEndian32(invalid.data() + 4, 0x20000000u);
    OSWriteBigEndian32(invalid.data() + 8, 0u);
    OSWriteBigEndian32(invalid.data() + 12, 20u);
    OSWriteBigEndian32(invalid.data() + 16, 20u);
    if (!WriteBytes(invalidPath, invalid)) {
        return 4;
    }
    dlCount = plCount = 0;
    dlOut = plOut = nullptr;
    const s32 result =
        GDReadDLFile(invalidPath, &dlCount, &plCount, &dlOut, &plOut);
    std::remove(invalidPath);
    return result == -2 && dlCount == 0u && plCount == 0u &&
                   dlOut == nullptr && plOut == nullptr
               ? 0
               : 5;
}
