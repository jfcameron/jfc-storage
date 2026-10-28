// © Joseph Cameron - MIT License

#include <jfc/storage/impl_flush.h>

#include <cerrno>

#include <fcntl.h>
#include <unistd.h>

using namespace jfc;

namespace {
    [[nodiscard]] std::error_code _last_error() {
        return {errno, std::generic_category()};
    }

    [[nodiscard]] std::error_code _flush(const std::filesystem::path &aPath, const int aFlags) {
        const int descriptor = ::open(aPath.c_str(), aFlags | O_CLOEXEC);

        if (descriptor < 0) return _last_error();

        std::error_code error;

#ifdef __APPLE__
        if (::fcntl(descriptor, F_FULLFSYNC) != 0 && ::fsync(descriptor) != 0) error = _last_error();
#else
        if (::fsync(descriptor) != 0) error = _last_error();
#endif
        ::close(descriptor);

        return error;
    }
}

std::error_code storage::_flush_file(const std::filesystem::path &aFile) {
    return _flush(aFile, O_WRONLY);
}

std::error_code storage::_flush_directory(const std::filesystem::path &aDirectory) {
    auto error = _flush(aDirectory, O_RDONLY | O_DIRECTORY);

    if (error == std::errc::invalid_argument) return {};

    return error;
}

std::error_code storage::_rename_replacing(const std::filesystem::path &aFrom, const std::filesystem::path &aTo) {
    std::error_code error;

    std::filesystem::rename(aFrom, aTo, error);

    return error;
}

std::error_code storage::_rename_flushed(const std::filesystem::path &aFrom, const std::filesystem::path &aTo) {
    if (const auto error = _rename_replacing(aFrom, aTo)) return error;

    std::error_code error;

    if (aFrom.parent_path() != aTo.parent_path()) {
        error = _flush_directory(aFrom.parent_path());

        if (error) return error;
    }

    return _flush_directory(aTo.parent_path());
}
