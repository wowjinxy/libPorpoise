#ifndef LIBPORPOISE_TESTS_SDK_DVD_FIXTURE_HPP
#define LIBPORPOISE_TESTS_SDK_DVD_FIXTURE_HPP

#include <dolphin/dvd.h>
#include <dolphin/os.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

class DvdFixture {
public:
    explicit DvdFixture(const char* testName)
        : root_(
              std::filesystem::temp_directory_path() /
              (std::string("libporpoise-") + testName + "-" +
               std::to_string(
                   std::chrono::steady_clock::now()
                       .time_since_epoch()
                       .count()
               ))
          )
    {
        std::error_code error;
        std::filesystem::create_directories(root_, error);
        valid_ = !error;
    }

    ~DvdFixture()
    {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    bool valid() const
    {
        return valid_;
    }

    bool write(
        const std::filesystem::path& relativePath,
        const std::vector<std::uint8_t>& bytes
    )
    {
        std::error_code error;
        std::filesystem::create_directories(
            (root_ / relativePath).parent_path(),
            error
        );
        if (error) {
            return false;
        }

        std::ofstream output(root_ / relativePath, std::ios::binary);
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size())
        );
        return output.good();
    }

    bool initialize()
    {
        const std::string rootString = root_.string();
        if (!DVDSetRootDirectory(rootString.c_str())) {
            return false;
        }
        OSInit();
        DVDInit();
        return true;
    }

private:
    std::filesystem::path root_;
    bool valid_ = false;
};

#endif
