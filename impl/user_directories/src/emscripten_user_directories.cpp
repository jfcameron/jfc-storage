// © Joseph Cameron - All Rights Reserved

#include <jfc/storage/impl_user_directories.h>

using namespace jfc;

std::filesystem::path storage::_user_data_root() {
    return "/persistent/data";
}

std::filesystem::path storage::_user_config_root() {
    return "/persistent/config";
}
