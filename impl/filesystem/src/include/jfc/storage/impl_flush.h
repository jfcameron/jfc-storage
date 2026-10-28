// © Joseph Cameron - MIT License

#ifndef JFC_STORAGE_IMPL_FLUSH_H
#define JFC_STORAGE_IMPL_FLUSH_H

#include <filesystem>
#include <system_error>

namespace jfc::storage {
    /// \brief return once a file's contents are on the disk
    [[nodiscard]] std::error_code _flush_file(const std::filesystem::path &aFile);

    /// \brief return once the names in a directory are on the disk
    [[nodiscard]] std::error_code _flush_directory(const std::filesystem::path &aDirectory);

    /// \brief rename aFrom over aTo
    [[nodiscard]] std::error_code _rename_replacing(const std::filesystem::path &aFrom,
        const std::filesystem::path &aTo);

    /// \brief rename aFrom over aTo, and return once the rename is on the disk
    [[nodiscard]] std::error_code _rename_flushed(const std::filesystem::path &aFrom,
        const std::filesystem::path &aTo);
}

#endif
