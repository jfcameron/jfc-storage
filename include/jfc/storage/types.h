// © Joseph Cameron - All Rights Reserved

#ifndef JFC_STORAGE_TYPES_H
#define JFC_STORAGE_TYPES_H

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace jfc::storage {
    class exception;
    class store;

    using store_ptr_type = std::unique_ptr<store>;

    using file_data_type = std::vector<std::byte>;

    using file_view_type = std::span<const std::byte>;
}

#endif
