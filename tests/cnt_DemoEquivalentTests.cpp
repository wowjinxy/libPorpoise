#include <revolution/cnt.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifndef SDK_CNT_DEMO_CASE
#error SDK_CNT_DEMO_CASE must identify the SDK demo inventory sequence
#endif

namespace {

namespace fs = std::filesystem;

bool Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

class ContentFixture {
public:
    ContentFixture() {
        const auto unique =
            std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = fs::temp_directory_path() /
                ("libporpoise-cnt-" + std::to_string(unique));
        std::error_code error;
        fs::create_directories(root_, error);
    }

    ~ContentFixture() {
        CNTHostClearContentRegistry();
        std::error_code error;
        fs::remove_all(root_, error);
    }

    fs::path MakeContent(const std::string& name) {
        const fs::path path = root_ / name;
        std::error_code error;
        fs::create_directories(path, error);
        return path;
    }

    void Write(
        const fs::path& content,
        const std::string& name,
        const std::vector<u8>& bytes) {
        std::ofstream output(content / name, std::ios::binary);
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }

private:
    fs::path root_;
};

std::vector<u8> Sequence(std::size_t count, u8 base) {
    std::vector<u8> result(count);
    for (std::size_t index = 0; index < count; ++index) {
        result[index] = static_cast<u8>(base + index);
    }
    return result;
}

bool TestContentReadSeek() {
    ContentFixture fixture;
    const fs::path content = fixture.MakeContent("content2");
    const std::vector<u8> first = Sequence(32, 0x20);
    const std::vector<u8> second = Sequence(64, 0x60);
    fixture.Write(content, "test1.txt", first);
    fixture.Write(content, "test2.txt", second);

    CNTInit();
    if (!Require(
            CNTHostRegisterContent(0, 2, content.string().c_str()),
            "failed to register current-title content")) {
        return false;
    }
    CNTHandle handle{};
    CNTFileInfo firstFile{};
    CNTFileInfo secondFirstHalf{};
    CNTFileInfo secondSecondHalf{};
    CNTFileInfo offsetFile{};
    if (!Require(
            CNTInitHandle(2, &handle, nullptr) == CNT_RESULT_OK,
            "CNTInitHandle failed") ||
        !Require(
            CNTOpen(&handle, "test1.txt", &firstFile) == CNT_RESULT_OK &&
                CNTOpen(&handle, "test2.txt", &secondFirstHalf) ==
                    CNT_RESULT_OK &&
                CNTOpen(&handle, "test2.txt", &secondSecondHalf) ==
                    CNT_RESULT_OK &&
                CNTOpen(&handle, "test2.txt", &offsetFile) ==
                    CNT_RESULT_OK,
            "CNTOpen failed for content files")) {
        return false;
    }

    std::vector<u8> firstActual(first.size());
    std::vector<u8> firstHalf(second.size() / 2);
    std::vector<u8> secondHalf(second.size() / 2);
    alignas(32) std::array<u8, 32> offsetActual{};
    alignas(32) std::array<u8, 32> currentActual{};
    const bool reads =
        Require(
            CNTGetLength(&firstFile) == first.size() &&
                CNTGetLength(&secondFirstHalf) == second.size(),
            "CNTGetLength returned the wrong file size") &&
        Require(
            CNTRead(
                &firstFile,
                firstActual.data(),
                static_cast<u32>(firstActual.size())) ==
                static_cast<s32>(firstActual.size()),
            "CNTRead failed for the first file") &&
        Require(
            CNTRead(
                &secondFirstHalf,
                firstHalf.data(),
                static_cast<u32>(firstHalf.size())) ==
                static_cast<s32>(firstHalf.size()),
            "CNTRead failed for the first half") &&
        Require(
            CNTSeek(
                &secondSecondHalf,
                static_cast<s32>(firstHalf.size()),
                CNT_SEEK_SET) == CNT_RESULT_OK &&
                CNTTell(&secondSecondHalf) ==
                    static_cast<s32>(firstHalf.size()),
            "CNTSeek/CNTTell failed at the midpoint") &&
        Require(
            CNTRead(
                &secondSecondHalf,
                secondHalf.data(),
                static_cast<u32>(secondHalf.size())) ==
                static_cast<s32>(secondHalf.size()),
            "CNTRead failed for the second half") &&
        Require(
            CNTSeek(&offsetFile, 16, CNT_SEEK_SET) == CNT_RESULT_OK &&
                CNTReadWithOffset(
                    &offsetFile,
                    offsetActual.data(),
                    static_cast<u32>(offsetActual.size()),
                    4) == static_cast<s32>(offsetActual.size()),
            "CNTReadWithOffset failed for a current-relative read") &&
        Require(
            std::equal(
                offsetActual.begin(),
                offsetActual.end(),
                second.begin() + 20) &&
                CNTTell(&offsetFile) == 16,
            "CNTReadWithOffset used an absolute offset or changed CNTTell") &&
        Require(
            CNTRead(
                &offsetFile,
                currentActual.data(),
                static_cast<u32>(currentActual.size())) ==
                    static_cast<s32>(currentActual.size()) &&
                std::equal(
                    currentActual.begin(),
                    currentActual.end(),
                    second.begin() + 16),
            "CNTReadWithOffset did not restore the underlying stream position");

    std::vector<u8> reconstructed;
    reconstructed.insert(
        reconstructed.end(),
        firstHalf.begin(),
        firstHalf.end());
    reconstructed.insert(
        reconstructed.end(),
        secondHalf.begin(),
        secondHalf.end());
    const bool result =
        reads &&
        Require(firstActual == first, "first CNT file bytes differ") &&
        Require(
            reconstructed == second,
            "split CNT reads did not reconstruct the second file");

    CNTClose(&firstFile);
    CNTClose(&secondFirstHalf);
    CNTClose(&secondSecondHalf);
    CNTClose(&offsetFile);
    CNTReleaseHandle(&handle);
    CNTShutdown();
    return result;
}

bool TestDataTitleIsolation() {
    constexpr u64 titleId = 0x0001000561303030ULL;
    ContentFixture fixture;
    const fs::path current = fixture.MakeContent("current-content1");
    const fs::path dataTitle = fixture.MakeContent("data-title-content1");
    fixture.Write(current, "test1.txt", Sequence(16, 0x10));
    const std::vector<u8> expected = Sequence(16, 0xa0);
    fixture.Write(dataTitle, "test1.txt", expected);

    CNTInit();
    CNTHostRegisterContent(0, 1, current.string().c_str());
    CNTHostRegisterContent(titleId, 1, dataTitle.string().c_str());
    CNTHandle handle{};
    CNTFileInfo file{};
    std::vector<u8> actual(expected.size());

    const bool result =
        Require(
            CNTInitHandleTitle(titleId, 1, &handle, nullptr) ==
                CNT_RESULT_OK,
            "CNTInitHandleTitle failed for registered data title") &&
        Require(
            handle.titleId == titleId,
            "data-title handle lost its title identity") &&
        Require(
            CNTOpen(&handle, "test1.txt", &file) == CNT_RESULT_OK,
            "data-title file open failed") &&
        Require(
            CNTRead(
                &file,
                actual.data(),
                static_cast<u32>(actual.size())) ==
                static_cast<s32>(actual.size()),
            "data-title file read failed") &&
        Require(
            actual == expected,
            "data-title read leaked current-title content");
    CNTClose(&file);
    CNTReleaseHandle(&handle);

    CNTHandle missing{};
    const bool missingRejected =
        CNTInitHandleTitle(titleId + 1, 1, &missing, nullptr) ==
        CNT_RESULT_NOT_FOUND;
    CNTShutdown();
    return
        result &&
        Require(
            missingRejected,
            "unregistered data title was unexpectedly accessible");
}

bool TestStrapLanguageContent() {
    ContentFixture fixture;
    const fs::path content = fixture.MakeContent("content3");
    const std::array<const char*, 7> names{
        "strapImage_jp_LZ.bin",
        "strapImage_En_LZ.bin",
        "strapImage_Fr_LZ.bin",
        "strapImage_Ge_LZ.bin",
        "strapImage_It_LZ.bin",
        "strapImage_Sp_LZ.bin",
        "strapImage_Du_LZ.bin",
    };
    for (std::size_t language = 0; language < names.size(); ++language) {
        fixture.Write(
            content,
            names[language],
            Sequence(24, static_cast<u8>(0x20 + language * 8)));
    }

    CNTInit();
    CNTHostRegisterContent(0, 3, content.string().c_str());
    CNTHandle handle{};
    CNTFileInfo file{};
    const std::vector<u8> expected = Sequence(24, 0x28);
    std::vector<u8> actual(expected.size());
    if (!Require(
            CNTInitHandle(3, &handle, nullptr) == CNT_RESULT_OK,
            "strap content handle initialization failed") ||
        !Require(
            CNTOpen(&handle, names[1], &file) == CNT_RESULT_OK,
            "English strap asset did not open")) {
        return false;
    }

    const bool readSelectedLanguage =
        CNTGetLength(&file) == expected.size() &&
        CNTRead(&file, actual.data(), actual.size()) ==
            static_cast<s32>(actual.size()) &&
        actual == expected;
    CNTClose(&file);

    CNTDir directory{};
    CNTDirEntry entry{};
    u32 languageFiles = 0;
    if (CNTOpenDir(&handle, "/", &directory)) {
        while (CNTReadDir(&directory, &entry)) {
            if (!entry.isDir) {
                ++languageFiles;
            }
        }
        CNTCloseDir(&directory);
    }

    CNTReleaseHandle(&handle);
    CNTShutdown();
    return
        Require(
            readSelectedLanguage,
            "strap language selection returned the wrong asset") &&
        Require(
            languageFiles == names.size(),
            "strap content did not expose all language assets");
}

}  // namespace

int main() {
#if SDK_CNT_DEMO_CASE == 23
    return TestContentReadSeek() ? 0 : 1;
#elif SDK_CNT_DEMO_CASE == 24
    return TestDataTitleIsolation() ? 0 : 1;
#elif SDK_CNT_DEMO_CASE == 25
    return TestStrapLanguageContent() ? 0 : 1;
#else
#error Unsupported SDK_CNT_DEMO_CASE
#endif
}
