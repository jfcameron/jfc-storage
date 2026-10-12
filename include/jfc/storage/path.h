// © Joseph Cameron - All Rights Reserved

#ifndef JFC_STORAGE_PATH_H
#define JFC_STORAGE_PATH_H

#include <optional>
#include <string>
#include <string_view>

namespace jfc::storage {
    [[nodiscard]] std::optional<std::string> path_refusal(std::string_view aPath);
}

#endif
