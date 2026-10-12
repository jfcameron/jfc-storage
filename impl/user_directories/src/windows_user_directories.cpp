// © Joseph Cameron - All Rights Reserved

#include <jfc/storage/exception.h>
#include <jfc/storage/impl_user_directories.h>

#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>

using namespace jfc;

std::filesystem::path storage::_user_data_root() {
    PWSTR folder = nullptr;

    const HRESULT result = ::SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, nullptr, &folder);

    if (FAILED(result)) {
        ::CoTaskMemFree(folder);

        throw storage::exception("jfc::storage: cannot find the roaming application data folder: HRESULT "
            + std::to_string(static_cast<unsigned long>(result)));
    }

    std::filesystem::path path(folder);

    ::CoTaskMemFree(folder);

    return path;
}

std::filesystem::path storage::_user_config_root() {
    return _user_data_root();
}
