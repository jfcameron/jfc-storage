// © Joseph Cameron - MIT License

#ifndef JFC_STORAGE_USER_DIRECTORIES_H
#define JFC_STORAGE_USER_DIRECTORIES_H

#include <filesystem>
#include <string_view>

namespace jfc::storage {
    /// \brief where a program keeps the user's data: saves, downloads, etc.
    [[nodiscard]] std::filesystem::path user_data_directory(std::string_view aProgramName);

    /// \brief where a program keeps the user's configuration
    [[nodiscard]] std::filesystem::path user_config_directory(std::string_view aProgramName);
}

#endif
