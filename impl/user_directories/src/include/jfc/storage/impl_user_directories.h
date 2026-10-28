// © Joseph Cameron - MIT License

#ifndef JFC_STORAGE_IMPL_USER_DIRECTORIES_H
#define JFC_STORAGE_IMPL_USER_DIRECTORIES_H

#include <filesystem>

namespace jfc::storage {
    [[nodiscard]] std::filesystem::path _user_data_root();

    [[nodiscard]] std::filesystem::path _user_config_root();
}

#endif
