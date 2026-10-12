// © Joseph Cameron - All Rights Reserved

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

    [[nodiscard]] std::filesystem::path _xdg(const char *const aVariable, const char *const aFallback) {
        if (const char *const value = std::getenv(aVariable); value && *value) {
            std::filesystem::path path(value);

            if (path.is_absolute()) return path;
        }

        return _home() / aFallback;
    }
}

std::filesystem::path storage::_user_data_root() {
    return _xdg("XDG_DATA_HOME", ".local/share");
}

std::filesystem::path storage::_user_config_root() {
    return _xdg("XDG_CONFIG_HOME", ".config");
}
