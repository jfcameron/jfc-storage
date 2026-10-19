// © Joseph Cameron - All Rights Reserved

#ifndef JFC_STORAGE_TEST_INCLUDE_H
#define JFC_STORAGE_TEST_INCLUDE_H

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <span>

namespace jfc_storage_test {
    [[nodiscard]] inline std::vector<std::byte> bytes(const std::string_view aText) {
        std::vector<std::byte> out;

        for (const char each : aText) out.push_back(static_cast<std::byte>(each));

        return out;
    }

    [[nodiscard]] inline std::string text(std::span<const std::byte> aBytes) {
        std::string out;

        for (const std::byte each : aBytes) out.push_back(static_cast<char>(each));

        return out;
    }

    [[nodiscard]] inline std::filesystem::path utf8_path(const std::string_view aText) {
        return std::filesystem::path(std::u8string(aText.begin(), aText.end()));
    }

    class scratch_directory final {
        std::filesystem::path mPath;

    public:
        explicit scratch_directory(const std::string_view aName)
        : mPath(std::filesystem::temp_directory_path() / ("jfc-storage-test-" + std::string(aName))) {
            std::filesystem::remove_all(mPath);
            std::filesystem::create_directories(mPath);
        }

        [[nodiscard]] const std::filesystem::path &path() const { return mPath; }

        scratch_directory(const scratch_directory &) = delete;
        scratch_directory &operator=(const scratch_directory &) = delete;

        ~scratch_directory() {
            std::error_code ignored;

            std::filesystem::remove_all(mPath, ignored);
        }
    };
}

#endif
