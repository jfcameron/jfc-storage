// © Joseph Cameron - MIT License

#include <jfc/storage/exception.h>
#include <jfc/storage/filesystem_store.h>
#include <jfc/storage/persistent_directory.h>
#include <jfc/storage/persistent_store.h>
#include <jfc/storage/store.h>

#include <emscripten/em_asm.h>
#include <emscripten/emscripten.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace jfc;

namespace {
    using told_type = std::function<void(bool)>;

    struct mounted final {
        bool loading = true; //<! the first load is not through yet
        bool loaded = false; //<! the first load is through, and succeeded
        bool synchronising = false;
        bool again = false; //<! a save was asked for while another was under way, or before the first load was through
        told_type onLoaded;
        std::vector<told_type> saving; //<! told when the save under way finishes
        std::vector<told_type> waiting; //<! told when the save asked for again finishes
    };

    std::map<std::string, std::unique_ptr<mounted>> g_Mounted;

    [[nodiscard]] std::string _key(const std::filesystem::path &aPath) {
        const auto text = aPath.lexically_normal().u8string();

        std::string out(text.begin(), text.end());

        if (out.size() > 1 && out.ends_with('/')) out.pop_back();

        return out;
    }

    [[nodiscard]] mounted &_mounted_or_throw(const std::filesystem::path &aPath, const char *const apOperation) {
        const auto found = g_Mounted.find(_key(aPath));

        if (found == g_Mounted.end())
            throw storage::exception("jfc::storage: cannot " + std::string(apOperation) + " \"" + _key(aPath)
                + "\": it is not mounted on IndexedDB; persistent_directory::mount it first");

        return *found->second;
    }

    void _tell(const std::vector<told_type> &aTold, const bool aSucceeded) {
        for (const auto &each : aTold) if (each) each(aSucceeded);
    }

    void _start_saving(mounted &aMounted) {
        aMounted.synchronising = true;
        aMounted.again = false;
        aMounted.saving = std::exchange(aMounted.waiting, {});

        EM_ASM({
            FS.syncfs(false, function(error) {
                _jfc_storage_persistent_saved($0, error ? 0 : 1);
            });
        }, &aMounted);
    }
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE void jfc_storage_persistent_loaded(mounted *const apMounted, const int aSucceeded) {
        apMounted->loading = false;
        apMounted->loaded = aSucceeded != 0;

        if (apMounted->again) {
            if (apMounted->loaded) _start_saving(*apMounted);
            else {
                apMounted->again = false;

                _tell(std::exchange(apMounted->waiting, {}), false);
            }
        }

        if (apMounted->onLoaded) apMounted->onLoaded(apMounted->loaded);
    }

    EMSCRIPTEN_KEEPALIVE void jfc_storage_persistent_saved(mounted *const apMounted, const int aSucceeded) {
        apMounted->synchronising = false;

        const auto told = std::exchange(apMounted->saving, {});

        if (apMounted->again) _start_saving(*apMounted);

        _tell(told, aSucceeded != 0);
    }
}

void storage::persistent_directory::mount(const std::filesystem::path &aPath, std::function<void(bool)> aOnLoaded) {
    const auto key = _key(aPath);

    if (g_Mounted.contains(key))
        throw storage::exception("jfc::storage: \"" + key + "\" is already mounted on IndexedDB");

    auto &pMounted = g_Mounted[key];

    pMounted = std::make_unique<mounted>();

    pMounted->onLoaded = std::move(aOnLoaded);

    const int ok = EM_ASM_INT({
        const path = UTF8ToString($0);

        try {
            FS.mkdirTree(path);
            FS.mount(IDBFS, {}, path);
        }
        catch (e) {
            return 0;
        }

        FS.syncfs(true, function(error) {
            _jfc_storage_persistent_loaded($1, error ? 0 : 1);
        });

        return 1;
    }, key.c_str(), pMounted.get());

    if (!ok) {
        g_Mounted.erase(key);

        throw storage::exception("jfc::storage: cannot mount \"" + key + "\" on IndexedDB");
    }
}

bool storage::persistent_directory::is_loaded(const std::filesystem::path &aPath) {
    const auto found = g_Mounted.find(_key(aPath));

    return found != g_Mounted.end() && found->second->loaded;
}

void storage::persistent_directory::synchronise(const std::filesystem::path &aPath, std::function<void(bool)> aOnSaved) {
    auto &mounted = _mounted_or_throw(aPath, "synchronise");

    if (!mounted.loading && !mounted.loaded) {
        if (aOnSaved) aOnSaved(false);

        return;
    }

    mounted.waiting.push_back(std::move(aOnSaved));

    if (mounted.loading || mounted.synchronising) {
        mounted.again = true;

        return;
    }

    _start_saving(mounted);
}

bool storage::persistent_directory::is_synchronising(const std::filesystem::path &aPath) {
    const auto found = g_Mounted.find(_key(aPath));

    return found != g_Mounted.end() && (found->second->synchronising || found->second->again);
}

namespace {
    class _persistent_store final : public storage::store {
    public:
        _persistent_store(storage::store_ptr_type aStore, std::string aMount, mounted &aMounted, told_type aOnSynchronised)
        : mStore(std::move(aStore))
        , mMount(std::move(aMount))
        , mMounted(aMounted)
        , mOnSynchronised(std::move(aOnSynchronised))
        {}

    private:
        storage::store_ptr_type mStore;

        std::string mMount;

        mounted &mMounted;

        told_type mOnSynchronised;

        void _before_writing(const std::string_view aOperation, const std::string &aPath) const {
            if (mMounted.loading)
                throw storage::exception("jfc::storage: cannot " + std::string(aOperation) + " \"" + aPath + "\": \""
                    + mMount + "\" has not loaded from IndexedDB yet, and its load would erase what is written now");
        }

        void _written() { storage::persistent_directory::synchronise(mMount, mOnSynchronised); }

        [[nodiscard]] std::optional<storage::file_data_type> _load_file(const std::string &aPath) const override {
            return mStore->load_file(aPath);
        }

        [[nodiscard]] bool _contains_file(const std::string &aPath) const override { return mStore->contains_file(aPath); }

        [[nodiscard]] std::optional<storage::file_info> _file_info(const std::string &aPath) const override {
            return mStore->file_info(aPath);
        }

        [[nodiscard]] bool _contains_directory(const std::string &aDirectory) const override {
            return mStore->contains_directory(aDirectory);
        }

        [[nodiscard]] std::vector<std::string> _files(const std::string &aDirectory) const override {
            return mStore->files(aDirectory);
        }

        [[nodiscard]] std::vector<std::string> _directories(const std::string &aDirectory) const override {
            return mStore->directories(aDirectory);
        }

        void _save_file(const std::string &aPath, const storage::file_view_type aData) override {
            _before_writing("save", aPath);

            mStore->save_file(aPath, aData);

            _written();
        }

        void _make_directory(const std::string &aDirectory) override {
            _before_writing("make the directory", aDirectory);

            mStore->make_directory(aDirectory);

            _written();
        }

        void _remove_file(const std::string &aPath) override {
            _before_writing("remove", aPath);

            mStore->remove_file(aPath);

            _written();
        }

        void _remove_directory(const std::string &aDirectory) override {
            _before_writing("remove the directory", aDirectory);

            mStore->remove_directory(aDirectory);

            _written();
        }

        void _move_file(const std::string &aOldPath, const std::string &aNewPath) override {
            _before_writing("move", aOldPath);

            mStore->move_file(aOldPath, aNewPath);

            _written();
        }

        void _move_directory(const std::string &aOldDirectory, const std::string &aNewDirectory) override {
            _before_writing("move the directory", aOldDirectory);

            mStore->move_directory(aOldDirectory, aNewDirectory);

            _written();
        }
    };
}

storage::store_ptr_type storage::make_persistent_store(std::filesystem::path aRoot, std::function<void(bool)> aOnSynchronised) {
    const auto root = _key(aRoot);

    const std::pair<const std::string, std::unique_ptr<mounted>> *pHolder = nullptr;

    for (const auto &each : g_Mounted) {
        const auto inside = each.first.ends_with('/') ? each.first : each.first + "/";

        if (root != each.first && !root.starts_with(inside)) continue;

        if (!pHolder || each.first.size() > pHolder->first.size()) pHolder = &each;
    }

    if (!pHolder)
        throw storage::exception("jfc::storage: cannot make a persistent store at \"" + root
            + "\": no directory mounted on IndexedDB holds it; persistent_directory::mount one first");

    return std::make_unique<_persistent_store>(make_filesystem_store(std::move(aRoot)), pHolder->first, *pHolder->second,
        std::move(aOnSynchronised));
}
