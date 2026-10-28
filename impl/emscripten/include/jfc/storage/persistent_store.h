// © Joseph Cameron - MIT License

#ifndef JFC_STORAGE_PERSISTENT_STORE_H
#define JFC_STORAGE_PERSISTENT_STORE_H

#include <jfc/storage/types.h>

#include <filesystem>
#include <functional>

namespace jfc::storage {
    /// \brief a filesystem store at aRoot, in a directory persistent_directory has mounted, that synchronises the mount
    /// after every save, move and removal
    [[nodiscard]] store_ptr_type make_persistent_store(std::filesystem::path aRoot,
        std::function<void(bool aSucceeded)> aOnSynchronised = {});
}

#endif
