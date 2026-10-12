// © Joseph Cameron - All Rights Reserved

#include <jfc/storage/exception.h>
#include <jfc/storage/impl_user_directories.h>

#include <cstdlib>

using namespace jfc;

namespace {
    [[nodiscard]] std::filesystem::path _container() {
        if (const char *const home = std::getenv("HOME"); home && *home) return home;

        throw storage::exception("jfc::storage: cannot find the app's container: HOME is unset");
    }
}

std::filesystem::path storage::_user_data_root() {
    return _container() / "Library" / "Application Support";
}

std::filesystem::path storage::_user_config_root() {
    return _user_data_root();
}
