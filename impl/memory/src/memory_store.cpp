// © Joseph Cameron - All Rights Reserved

#include <jfc/storage/exception.h>
#include <jfc/storage/memory_store.h>
#include <jfc/storage/store.h>

#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

using namespace jfc;

namespace {
    [[nodiscard]] std::string _parent_of(const std::string &aPath) {
        const auto slash = aPath.rfind('/');

        return slash == std::string::npos ? std::string() : aPath.substr(0, slash);
    }

    template<typename Paths, typename PathOf>
    [[nodiscard]] std::vector<std::string> _directly_in(const Paths &aPaths, const std::string &aDirectory,
        const PathOf aPathOf) {
        const auto prefix = aDirectory.empty() ? std::string() : aDirectory + "/";

        std::vector<std::string> out;

        for (auto each = aPaths.lower_bound(prefix); each != aPaths.end(); ++each) {
            const std::string &path = aPathOf(*each);

            if (!path.starts_with(prefix)) break;

            const auto name = std::string_view(path).substr(prefix.size());

            if (!name.empty() && name.find('/') == std::string_view::npos) out.emplace_back(name);
        }

        return out;
    }

    [[nodiscard]] bool _is_in(const std::string &aPath, const std::string &aDirectory) {
        return aPath == aDirectory || aPath.starts_with(aDirectory + "/");
    }

    class _memory_store final : public storage::store {
        std::map<std::string, storage::file_data_type> mFiles;

        std::set<std::string> mDirectories; 

        [[nodiscard]] storage::exception _failed(const std::string_view aOperation, const std::string &aPath,
            const std::string &aWhy) const {
            return storage::exception("jfc::storage: cannot " + std::string(aOperation) + " \"" + aPath + "\": "
                + aWhy);
        }

        void _make_directories_for(const std::string_view aOperation, const std::string &aPath) {
            std::vector<std::string> made;

            for (auto directory = _parent_of(aPath); !directory.empty(); directory = _parent_of(directory)) {
                if (mFiles.contains(directory))
                    throw _failed(aOperation, aPath, "\"" + directory + "\" is a file, not a directory");

                made.push_back(directory);
            }

            mDirectories.insert(made.begin(), made.end());
        }

        [[nodiscard]] std::optional<storage::file_data_type> _load_file(const std::string &aPath) const override {
            const auto found = mFiles.find(aPath);

            if (found == mFiles.end()) return std::nullopt;

            return found->second;
        }

        [[nodiscard]] bool _contains_file(const std::string &aPath) const override {
            return mFiles.contains(aPath);
        }

        [[nodiscard]] bool _contains_directory(const std::string &aDirectory) const override {
            return mDirectories.contains(aDirectory);
        }

        [[nodiscard]] std::vector<std::string> _files(const std::string &aDirectory) const override {
            return _directly_in(mFiles, aDirectory, [](const auto &aEach) -> const std::string & {
                return aEach.first;
            });
        }

        [[nodiscard]] std::vector<std::string> _directories(const std::string &aDirectory) const override {
            return _directly_in(mDirectories, aDirectory, [](const std::string &aEach) -> const std::string & {
                return aEach;
            });
        }

        void _save_file(const std::string &aPath, const storage::file_view_type aData) override {
            if (mDirectories.contains(aPath)) throw _failed("save", aPath, "a directory is there");

            storage::file_data_type contents(aData.begin(), aData.end());

            _make_directories_for("save", aPath);

            mFiles.insert_or_assign(aPath, std::move(contents));
        }

        void _remove_file(const std::string &aPath) override {
            mFiles.erase(aPath);
        }

        void _remove_directory(const std::string &aDirectory) override {
            std::erase_if(mFiles, [&](const auto &aEach) { return _is_in(aEach.first, aDirectory); });
            std::erase_if(mDirectories, [&](const auto &aEach) { return _is_in(aEach, aDirectory); });
        }

        void _move_file(const std::string &aOldPath, const std::string &aNewPath) override {
            if (aOldPath == aNewPath) return;

            _make_directories_for("move", aNewPath);

            auto contents = std::move(mFiles.at(aOldPath));

            mFiles.erase(aOldPath);

            mFiles.insert_or_assign(aNewPath, std::move(contents));
        }

        void _move_directory(const std::string &aOldDirectory, const std::string &aNewDirectory) override {
            _make_directories_for("move the directory", aNewDirectory);

            const auto renamed = [&](const std::string &aPath) {
                return aNewDirectory + aPath.substr(aOldDirectory.size());
            };

            std::map<std::string, storage::file_data_type> files;

            for (auto each = mFiles.begin(); each != mFiles.end();) {
                if (_is_in(each->first, aOldDirectory)) {
                    files.emplace(renamed(each->first), std::move(each->second));

                    each = mFiles.erase(each);
                }
                else ++each;
            }

            std::set<std::string> directories;

            for (auto each = mDirectories.begin(); each != mDirectories.end();) {
                if (_is_in(*each, aOldDirectory)) {
                    directories.insert(renamed(*each));

                    each = mDirectories.erase(each);
                }
                else ++each;
            }

            mFiles.merge(files);
            mDirectories.merge(directories);
        }
    };
}

storage::store_ptr_type storage::make_memory_store() {
    return std::make_unique<_memory_store>();
}
