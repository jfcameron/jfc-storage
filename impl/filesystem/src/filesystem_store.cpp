// © Joseph Cameron - All Rights Reserved

#include <jfc/storage/exception.h>
#include <jfc/storage/filesystem_store.h>
#include <jfc/storage/impl_flush.h>
#include <jfc/storage/path.h>
#include <jfc/storage/store.h>

#include <algorithm>
#include <fstream>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

using namespace jfc;

namespace {
    [[nodiscard]] std::filesystem::path _native(std::string_view aPath) {
        return std::filesystem::path(std::u8string(aPath.begin(), aPath.end()));
    }

    [[nodiscard]] std::string _utf8(const std::filesystem::path &aPath) {
        const auto text = aPath.u8string();

        return std::string(text.begin(), text.end());
    }

    void _restrict_to_owner([[maybe_unused]] const std::filesystem::path &aPath,
        [[maybe_unused]] const bool aIsDirectory) {
#ifndef _WIN32
        std::error_code error;

        std::filesystem::permissions(aPath,
            aIsDirectory
                ? std::filesystem::perms::owner_all
                : std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
            std::filesystem::perm_options::replace,
            error);

        if (error) throw storage::exception("jfc::storage: cannot restrict \"" + _utf8(aPath)
            + "\" to its owner: " + error.message());
#endif
    }

    class _filesystem_store final : public storage::store {
    public:
        _filesystem_store(std::filesystem::path aRoot, const storage::filesystem_store_config &aConfig)
        : mRoot(std::move(aRoot))
        , mConfig(aConfig) {
            if (!mConfig.restrict_to_owner) return;

            std::error_code error;

            std::filesystem::create_directories(mRoot, error);

            if (error) throw _failed("make", "", "cannot make the root", error);

            _restrict_to_owner(mRoot, true);
        }

    private:
        std::filesystem::path mRoot;

        storage::filesystem_store_config mConfig;

        [[nodiscard]] storage::exception _failed(const std::string_view aOperation, std::string_view aPath,
            const std::string_view aWhy, const std::error_code aError = {}) const {
            std::string what = "jfc::storage: cannot " + std::string(aOperation) + " \"" + std::string(aPath) + "\" in \""
                + _utf8(mRoot) + "\": " + std::string(aWhy);

            if (aError) what += ": " + aError.message();

            return storage::exception(what);
        }

        [[nodiscard]] std::filesystem::path _where(const std::string &aPath) const {
            return aPath.empty() ? mRoot : mRoot / _native(aPath);
        }

        [[nodiscard]] std::filesystem::file_status _status(const std::string_view aOperation,
            const std::string &aPath) const {
            std::error_code error;

            const auto status = std::filesystem::status(_where(aPath), error);

            if (status.type() == std::filesystem::file_type::none)
                throw _failed(aOperation, aPath, "cannot tell what is there", error);

            return status;
        }

        void _make_directory_for(const std::string_view aOperation, const std::string &aPath) {
            const auto directory = _where(aPath).parent_path();

            std::error_code error;

            std::vector<std::filesystem::path> made;

            if (mConfig.flush_to_disk) {
                for (auto each = directory; each != each.parent_path() && !std::filesystem::exists(each, error);
                    each = each.parent_path())
                    made.push_back(each);
            }

            std::filesystem::create_directories(directory, error);

            if (error) throw _failed(aOperation, aPath, "cannot make its directory", error);

            if (mConfig.restrict_to_owner) _restrict_to_owner(directory, true);

            for (const auto &each : made) {
                error = storage::_flush_directory(each.parent_path());

                if (error) throw _failed(aOperation, aPath, "cannot flush its directory to the disk", error);
            }
        }

        [[nodiscard]] std::error_code _rename(const std::filesystem::path &aFrom, const std::filesystem::path &aTo) {
            if (mConfig.flush_to_disk) return storage::_rename_flushed(aFrom, aTo);

            std::error_code error;

            std::filesystem::rename(aFrom, aTo, error);

            return error;
        }

        [[nodiscard]] std::optional<storage::file_data_type> _load_file(const std::string &aPath) const override {
            if (!std::filesystem::is_regular_file(_status("load", aPath))) return std::nullopt;

            const auto where = _where(aPath);

            if (mConfig.restrict_to_owner) _restrict_to_owner(where, false);

            std::ifstream file(where, std::ios::binary | std::ios::ate);

            if (!file) throw _failed("load", aPath, "cannot open it");

            const auto size = file.tellg();

            if (size < 0) throw _failed("load", aPath, "cannot tell its size");

            storage::file_data_type contents(static_cast<std::size_t>(size));

            file.seekg(0, std::ios::beg);

            if (!file.read(reinterpret_cast<char *>(contents.data()), static_cast<std::streamsize>(contents.size())))
                throw _failed("load", aPath, "cannot read it");

            return contents;
        }

        void _save_file(const std::string &aPath, const storage::file_view_type aData) override {
            _make_directory_for("save", aPath);

            const auto where = _where(aPath);

            auto temporaryName = std::filesystem::path(".jfc-storage-saving-");
            temporaryName += where.filename();

            const auto temporary = where.parent_path() / temporaryName;

            try {
                {
                    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);

                    if (!file) throw _failed("save", aPath, "cannot open a temporary file to write");

                    file.write(reinterpret_cast<const char *>(aData.data()),
                        static_cast<std::streamsize>(aData.size()));

                    file.close();

                    if (!file) throw _failed("save", aPath, "cannot write the temporary file");
                }

                if (mConfig.restrict_to_owner) _restrict_to_owner(temporary, false);

                if (mConfig.flush_to_disk) {
                    if (const auto error = storage::_flush_file(temporary))
                        throw _failed("save", aPath, "cannot flush the temporary file to the disk", error);
                }

                if (const auto error = _rename(temporary, where))
                    throw _failed("save", aPath, "cannot replace the file", error);
            }
            catch (...) {
                std::error_code ignored;

                std::filesystem::remove(temporary, ignored);

                throw;
            }
        }

        void _remove_file(const std::string &aPath) override {
            std::error_code error;

            std::filesystem::remove(_where(aPath), error);

            if (error) throw _failed("remove", aPath, "cannot remove it", error);
        }

        void _remove_directory(const std::string &aDirectory) override {
            std::error_code error;

            std::filesystem::remove_all(_where(aDirectory), error);

            if (error) throw _failed("remove the directory", aDirectory, "cannot remove it", error);
        }

        void _move_file(const std::string &aOldPath, const std::string &aNewPath) override {
            _make_directory_for("move", aNewPath);

            if (const auto error = _rename(_where(aOldPath), _where(aNewPath)))
                throw _failed("move", aOldPath, "cannot move it to \"" + aNewPath + "\"", error);
        }

        void _move_directory(const std::string &aOldDirectory, const std::string &aNewDirectory) override {
            _make_directory_for("move the directory", aNewDirectory);

            if (const auto error = _rename(_where(aOldDirectory), _where(aNewDirectory)))
                throw _failed("move the directory", aOldDirectory, "cannot move it to \"" + aNewDirectory + "\"",
                    error);
        }

        [[nodiscard]] bool _contains_file(const std::string &aPath) const override {
            return std::filesystem::is_regular_file(_status("look for the file", aPath));
        }

        [[nodiscard]] bool _contains_directory(const std::string &aDirectory) const override {
            return std::filesystem::is_directory(_status("look for the directory", aDirectory));
        }

        [[nodiscard]] std::vector<std::string> _entries(const std::string_view aOperation,
            const std::string &aDirectory, const std::filesystem::file_type aType) const {
            if (!std::filesystem::is_directory(_status(aOperation, aDirectory))) return {};

            std::error_code error;

            std::filesystem::directory_iterator each(_where(aDirectory), error);

            if (error) throw _failed(aOperation, aDirectory, "cannot read the directory", error);

            std::vector<std::string> out;

            for (; each != std::filesystem::directory_iterator(); each.increment(error)) {
                if (error) throw _failed(aOperation, aDirectory, "cannot read the directory", error);

                std::error_code entryError;

                if (each->symlink_status(entryError).type() != aType) continue;

                auto name = _utf8(each->path().filename());

                if (storage::path_refusal(name)) continue;

                out.push_back(std::move(name));
            }

            if (error) throw _failed(aOperation, aDirectory, "cannot read the directory", error);

            std::sort(out.begin(), out.end());

            return out;
        }

        [[nodiscard]] std::vector<std::string> _files(const std::string &aDirectory) const override {
            return _entries("list the files in", aDirectory, std::filesystem::file_type::regular);
        }

        [[nodiscard]] std::vector<std::string> _directories(const std::string &aDirectory) const override {
            return _entries("list the directories in", aDirectory, std::filesystem::file_type::directory);
        }
    };
}

storage::store_ptr_type storage::make_filesystem_store(std::filesystem::path aRoot,
    const filesystem_store_config &aConfig) {
    if (aRoot.empty()) throw storage::exception("jfc::storage: cannot make a filesystem store: the root is empty");

    std::error_code error;

    auto root = std::filesystem::absolute(aRoot, error);

    if (error) throw storage::exception("jfc::storage: cannot make a filesystem store at \"" + _utf8(aRoot)
        + "\": " + error.message());

    return std::make_unique<_filesystem_store>(root.lexically_normal(), aConfig);
}
