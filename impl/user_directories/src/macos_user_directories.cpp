// © Joseph Cameron - MIT License

#include <jfc/storage/exception.h>
#include <jfc/storage/impl_user_directories.h>

#include <cstdlib>

#include <pwd.h>
#include <unistd.h>

using namespace jfc;

namespace {
    [[nodiscard]] std::filesystem::path _home() {
        if (const char *const home = std::getenv("HOME"); home && *home) return home;

        if (const passwd *const entry = ::getpwuid(::getuid()); entry && entry->pw_dir && *entry->pw_dir)
            return entry->pw_dir;

        throw storage::exception("jfc::storage: cannot find a home directory: HOME is unset, and this user has "
            "none on record");
    }
}

std::filesystem::path storage::_user_data_root() {
    return _home() / "Library" / "Application Support";
}

std::filesystem::path storage::_user_config_root() {
    return _home() / "Library" / "Preferences";
}
