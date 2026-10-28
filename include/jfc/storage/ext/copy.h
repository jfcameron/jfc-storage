// © Joseph Cameron - MIT License

#ifndef JFC_STORAGE_EXT_COPY_H
#define JFC_STORAGE_EXT_COPY_H

#include <jfc/storage/store.h>

#include <string_view>

namespace jfc::storage::ext {
    /// \brief copy a file, replacing any file already at aToPath
    void copy_file(const store &aFrom, std::string_view aFromPath, store &aTo, std::string_view aToPath);

    /// \brief copy a directory and everything in it
    void copy_directory(const store &aFrom, std::string_view aFromDirectory, store &aTo,
        std::string_view aToDirectory);
}

#endif
