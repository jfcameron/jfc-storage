// © Joseph Cameron - MIT License

#include <jfc/storage/exception.h>
#include <jfc/storage/impl_user_directories.h>
#include <jfc/storage/path.h>
#include <jfc/storage/user_directories.h>

#include <string>

using namespace jfc;

namespace {
    [[nodiscard]] std::filesystem::path _named(const std::string_view aProgramName) {
        if (aProgramName.empty() || aProgramName == "." || aProgramName.find('/') != std::string_view::npos)
            throw storage::exception("jfc::storage: \"" + std::string(aProgramName) + "\" cannot name a program's "
                "directory: it must be one name, and not \".\" or \"..\"");

        if (const auto refusal = storage::path_refusal(aProgramName))
            throw storage::exception("jfc::storage: \"" + std::string(aProgramName) + "\" cannot name a program's "
                "directory: " + *refusal);

        return std::filesystem::path(std::u8string(aProgramName.begin(), aProgramName.end()));
    }
}

std::filesystem::path storage::user_data_directory(const std::string_view aProgramName) {
    const auto name = _named(aProgramName);

    return _user_data_root() / name;
}

std::filesystem::path storage::user_config_directory(const std::string_view aProgramName) {
    const auto name = _named(aProgramName);

    return _user_config_root() / name;
}
