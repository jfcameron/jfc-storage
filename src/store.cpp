// © Joseph Cameron - All Rights Reserved

#include <jfcstorage/buildinfo.h> 

#include <jfc/storage/exception.h>
#include <jfc/storage/store.h>

#include <fstream>

using namespace jfc::storage;

namespace {
#ifdef JFC_TARGET_PLATFORM_Linux 
    std::filesystem::path get_path(std::string_view aProgramName) {
        const char *dataHome = std::getenv("XDG_DATA_HOME");
        if (dataHome && *dataHome)
            return std::filesystem::path(dataHome) / aProgramName;

        const char *home = std::getenv("HOME");
        if (!home) throw jfc::storage::exception("HOME not found in environment");

        std::filesystem::path root = std::filesystem::path(home) 
            / ".local"
            / "share"
            / aProgramName;

        return root;
    }
#elif defined JFC_TARGET_PLATFORM_Darwin
    std::filesystem::path get_path(std::string_view aProgramName) {
        const char *home = std::getenv("HOME");
        if (!home) throw jfc::storage::exception("HOME not found in environment");

        std::filesystem::path root = std::filesystem::path(home) 
            / "Library"
            / "Application Support"
            / aProgramName;

        return root;
    }
#elif defined JFC_TARGET_PLATFORM_Windows
    std::filesystem::path get_path(std::string_view aProgramName) {
        const char *appData = std::getenv("APPDATA");
        if (!appData) throw jfc::storage::exception("APPDATA not found in environment");

        return std::filesystem::path(appData)
            / aProgramName;
    }
#endif
}

store::store(std::string_view aProgramName) 
: mRoot(get_path(aProgramName))
{}

store store::make_from_root(std::filesystem::path aRoot) {
    store newStore;
    newStore.mRoot = aRoot;
    return newStore;
}

std::optional<std::vector<std::byte>> store::load_file(std::string_view aPath) const {
    const auto path = resolve_path(aPath);
    if (!path) throw jfc::storage::exception("path escapes storage root");

    std::ifstream file(*path, std::ios::binary | std::ios::ate);
    if (!file) {
        if (!std::filesystem::exists(*path)) return std::nullopt;
        throw jfc::storage::exception("failed to open file");
    }

    const auto size = file.tellg();
    if (size < 0) return std::nullopt;

    std::vector<std::byte> contents(static_cast<std::size_t>(size));

    file.seekg(0, std::ios::beg);

    if (!file.read(reinterpret_cast<char *>(contents.data()),
        static_cast<std::streamsize>(contents.size())))
        return std::nullopt;

    return contents;
}

void store::save_file(std::string_view aPath, std::span<const std::byte> aData) {
    const auto path = resolve_path(aPath);
    if (!path) throw jfc::storage::exception("path escapes storage root");

    {
        std::error_code error;
        std::filesystem::create_directories(path->parent_path(), error);
        if (error) throw jfc::storage::exception("failed to create storage directory");
    }

    auto temporaryPath = *path;
    temporaryPath += ".tmp";

    {
        std::ofstream file(
            temporaryPath,
            std::ios::binary | std::ios::trunc);

        if (!file)
            throw jfc::storage::exception("failed to open temporary file for writing");

        file.write(
            reinterpret_cast<const char *>(aData.data()),
            static_cast<std::streamsize>(aData.size()));

        if (!file)
            throw jfc::storage::exception("failed to write temporary file");
    }

    std::error_code error;
    std::filesystem::rename(temporaryPath, *path, error);
    if (error) throw jfc::storage::exception( "failed to replace file");
}

std::optional<std::filesystem::path> store::resolve_path(std::string_view aPath) const {
    const std::filesystem::path relative(aPath);

    if (relative.is_absolute()) return std::nullopt;

    const auto root = mRoot.lexically_normal();
    const auto path = (root / relative).lexically_normal();

    auto rootIt = root.begin();
    auto pathIt = path.begin();

    for (; rootIt != root.end(); ++rootIt, ++pathIt)
        if (pathIt == path.end() || *rootIt != *pathIt) return std::nullopt;

    return path;
}

void store::remove_file(std::string_view aPath)
{
    const auto path = resolve_path(aPath);
    if (!path) throw jfc::storage::exception("path escapes storage root");

    std::error_code error;
    std::filesystem::remove(*path, error);
    if (error) throw jfc::storage::exception("failed to remove file");
}

void store::move_file(std::string_view aOldPath, std::string_view aNewPath) {
    const auto oldPath = resolve_path(aOldPath);
    if (!oldPath) throw jfc::storage::exception("source path escapes storage root");

    const auto newPath = resolve_path(aNewPath);
    if (!newPath) throw jfc::storage::exception("destination path escapes storage root");

    std::error_code error;
    std::filesystem::rename(*oldPath, *newPath, error);
    if (error) throw jfc::storage::exception("failed to move file");
}
