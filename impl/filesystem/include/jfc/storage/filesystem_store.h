// © Joseph Cameron - All Rights Reserved

#ifndef JFC_STORAGE_FILESYSTEM_STORE_H
#define JFC_STORAGE_FILESYSTEM_STORE_H

#include <jfc/storage/types.h>

#include <filesystem>

namespace jfc::storage {
    struct filesystem_store_config {
        /// \brief let only this user read or write the store's directories and files
        bool restrict_to_owner = false;

        /// \brief return from each save and move only once it is on the disk
        bool flush_to_disk = false;
    };

    /// \brief a store whose files are files under aRoot
    [[nodiscard]] store_ptr_type make_filesystem_store(std::filesystem::path aRoot,
        const filesystem_store_config &aConfig = {});
}

#endif
