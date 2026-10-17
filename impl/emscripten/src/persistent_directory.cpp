// © Joseph Cameron - All Rights Reserved

#include <jfc/storage/exception.h>
#include <jfc/storage/persistent_directory.h>

#include <emscripten/em_asm.h>
#include <emscripten/emscripten.h>

#include <map>
#include <memory>
#include <string>

using namespace jfc;

namespace {
    struct mounted final {
        bool loaded = false;
        bool synchronising = false;
        std::function<void(bool)> onLoaded;
    };

    std::map<std::string, std::unique_ptr<mounted>> g_Mounted;

    [[nodiscard]] std::string _key(const std::filesystem::path &aPath) {
        const auto text = aPath.lexically_normal().u8string();

        return std::string(text.begin(), text.end());
    }

    [[nodiscard]] mounted &_mounted_or_throw(const std::filesystem::path &aPath, const char *const apOperation) {
        const auto found = g_Mounted.find(_key(aPath));

        if (found == g_Mounted.end())
            throw storage::exception("jfc::storage: cannot " + std::string(apOperation) + " \"" + _key(aPath)
                + "\": it is not mounted on IndexedDB; persistent_directory::mount it first");

        return *found->second;
    }
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE void jfc_storage_persistent_loaded(mounted *const apMounted, const int aSucceeded) {
        apMounted->loaded = aSucceeded != 0;

        if (apMounted->onLoaded) apMounted->onLoaded(aSucceeded != 0);
    }

    EMSCRIPTEN_KEEPALIVE void jfc_storage_persistent_saved(mounted *const apMounted) {
        apMounted->synchronising = false;
    }
}

void storage::persistent_directory::mount(const std::filesystem::path &aPath, std::function<void(bool)> aOnLoaded) {
    const auto key = _key(aPath);

    if (g_Mounted.contains(key))
        throw storage::exception("jfc::storage: \"" + key + "\" is already mounted on IndexedDB");

    auto pMounted = std::make_unique<mounted>();

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

    if (!ok) throw storage::exception("jfc::storage: cannot mount \"" + key + "\" on IndexedDB");

    g_Mounted[key] = std::move(pMounted);
}

bool storage::persistent_directory::is_loaded(const std::filesystem::path &aPath) {
    const auto found = g_Mounted.find(_key(aPath));

    return found != g_Mounted.end() && found->second->loaded;
}

void storage::persistent_directory::synchronise(const std::filesystem::path &aPath) {
    auto &mounted = _mounted_or_throw(aPath, "synchronise");

    if (mounted.synchronising) return;

    mounted.synchronising = true;

    EM_ASM({
        FS.syncfs(false, function(error) {
            _jfc_storage_persistent_saved($0);
        });
    }, &mounted);
}

bool storage::persistent_directory::is_synchronising(const std::filesystem::path &aPath) {
    const auto found = g_Mounted.find(_key(aPath));

    return found != g_Mounted.end() && found->second->synchronising;
}
