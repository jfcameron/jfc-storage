// © Joseph Cameron - MIT License

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

    [[nodiscard]] std::error_code _move(const std::filesystem::path &aFrom, const std::filesystem::path &aTo,
        const DWORD aFlags) {
        for (DWORD wait = 1;; wait *= 2) {
            if (::MoveFileExW(aFrom.c_str(), aTo.c_str(), MOVEFILE_REPLACE_EXISTING | aFlags)) return {};

            const DWORD error = ::GetLastError();

            if ((error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION) || wait > 512)
                return {static_cast<int>(error), std::system_category()};

            ::Sleep(wait);
        }
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

std::error_code storage::_flush_directory(const std::filesystem::path &) {
    return {};
}

std::error_code storage::_rename_replacing(const std::filesystem::path &aFrom, const std::filesystem::path &aTo) {
    return _move(aFrom, aTo, 0);
}

std::error_code storage::_rename_flushed(const std::filesystem::path &aFrom, const std::filesystem::path &aTo) {
    return _move(aFrom, aTo, MOVEFILE_WRITE_THROUGH);
}
