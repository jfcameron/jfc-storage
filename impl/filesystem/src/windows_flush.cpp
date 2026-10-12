// © Joseph Cameron - All Rights Reserved

#include <jfc/storage/impl_flush.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

using namespace jfc;

namespace {
    [[nodiscard]] std::error_code _last_error() {
        return {static_cast<int>(::GetLastError()), std::system_category()};
    }
}

std::error_code storage::_flush_file(const std::filesystem::path &aFile) {
    const HANDLE file = ::CreateFileW(aFile.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (file == INVALID_HANDLE_VALUE) return _last_error();

    std::error_code error;

    if (!::FlushFileBuffers(file)) error = _last_error();

    ::CloseHandle(file);

    return error;
}

// Windows offers no flush of a directory to a program that is not an administrator. NTFS journals the names it
// holds, and a rename asks to be written through (_rename_flushed), which is as much as can be done.
std::error_code storage::_flush_directory(const std::filesystem::path &) {
    return {};
}

std::error_code storage::_rename_flushed(const std::filesystem::path &aFrom, const std::filesystem::path &aTo) {
    if (!::MoveFileExW(aFrom.c_str(), aTo.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return _last_error();

    return {};
}
