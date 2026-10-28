// © Joseph Cameron - MIT License

#include "test_include.h"

#include <jfc/catch.hpp>

#include <jfc/storage/exception.h>
#include <jfc/storage/filesystem_store.h>
#include <jfc/storage/persistent_directory.h>
#include <jfc/storage/persistent_store.h>
#include <jfc/storage/store.h>

#include <emscripten/em_asm.h>

#include <optional>
#include <string>
#include <vector>

using namespace jfc;
using namespace jfc_storage_test;

namespace {
    class held_syncs final {
    public:
        held_syncs(const held_syncs &) = delete;
        held_syncs &operator=(const held_syncs &) = delete;

        held_syncs() {
            EM_ASM({
                Module.jfcHeld = [];
                Module.jfcRealSyncfs = FS.syncfs;

                FS.syncfs = function(populate, callback) {
                    Module.jfcHeld.push({populate: populate, callback: callback});
                };
            });
        }

        ~held_syncs() {
            EM_ASM({ FS.syncfs = Module.jfcRealSyncfs; });
        }

        [[nodiscard]] int count() const { return EM_ASM_INT({ return Module.jfcHeld.length; }); }

        [[nodiscard]] bool next_is_a_load() const { return EM_ASM_INT({ return Module.jfcHeld[0].populate ? 1 : 0; }); }

        void finish_next(const bool aSucceeded) {
            EM_ASM({
                const held = Module.jfcHeld.shift();

                held.callback($0 ? null : new Error("refused"));
            }, aSucceeded);
        }
    };
}

TEST_CASE("a save asked for before the first load is through waits for it",
    "[jfc::storage::persistent_directory]") {
    held_syncs held;

    std::optional<bool> loaded, saved;

    storage::persistent_directory::mount("/persistent-waits", [&](const bool aSucceeded) { loaded = aSucceeded; });

    REQUIRE(held.count() == 1);
    REQUIRE(held.next_is_a_load());

    storage::persistent_directory::synchronise("/persistent-waits", [&](const bool aSucceeded) { saved = aSucceeded; });

    REQUIRE(held.count() == 1);
    REQUIRE(storage::persistent_directory::is_synchronising("/persistent-waits"));

    held.finish_next(true);

    REQUIRE(loaded == true);
    REQUIRE(held.count() == 1);
    REQUIRE_FALSE(held.next_is_a_load());

    held.finish_next(true);

    REQUIRE(saved == true);
    REQUIRE_FALSE(storage::persistent_directory::is_synchronising("/persistent-waits"));
    REQUIRE(held.count() == 0);
}

TEST_CASE("a save asked for while one is under way starts when it finishes, and each is told how it went",
    "[jfc::storage::persistent_directory]") {
    held_syncs held;

    storage::persistent_directory::mount("/persistent-queues", {});

    held.finish_next(true);

    std::optional<bool> first, second, third;

    storage::persistent_directory::synchronise("/persistent-queues", [&](const bool aSucceeded) { first = aSucceeded; });

    REQUIRE(held.count() == 1);

    storage::persistent_directory::synchronise("/persistent-queues", [&](const bool aSucceeded) { second = aSucceeded; });
    storage::persistent_directory::synchronise("/persistent-queues", [&](const bool aSucceeded) { third = aSucceeded; });

    REQUIRE(held.count() == 1);

    held.finish_next(true);

    REQUIRE(first == true);
    REQUIRE_FALSE(second);
    REQUIRE(held.count() == 1);
    REQUIRE(storage::persistent_directory::is_synchronising("/persistent-queues"));

    held.finish_next(false);

    REQUIRE(second == false);
    REQUIRE(third == false);
    REQUIRE(held.count() == 0);
    REQUIRE_FALSE(storage::persistent_directory::is_synchronising("/persistent-queues"));
}

TEST_CASE("after a load that failed nothing is saved, and a save says so", "[jfc::storage::persistent_directory]") {
    held_syncs held;

    std::optional<bool> waiting, after;

    storage::persistent_directory::mount("/persistent-unread", {});

    storage::persistent_directory::synchronise("/persistent-unread", [&](const bool aSucceeded) { waiting = aSucceeded; });

    held.finish_next(false);

    REQUIRE(waiting == false);

    storage::persistent_directory::synchronise("/persistent-unread", [&](const bool aSucceeded) { after = aSucceeded; });

    REQUIRE(after == false);
    REQUIRE(held.count() == 0);
    REQUIRE_FALSE(storage::persistent_directory::is_synchronising("/persistent-unread"));
}

TEST_CASE("under node, which has no IndexedDB, the load fails and nothing is saved",
    "[jfc::storage::persistent_directory]") {
    std::optional<bool> loaded, saved;

    storage::persistent_directory::mount("/persistent-real", [&](const bool aSucceeded) {
        loaded = aSucceeded;

        storage::persistent_directory::synchronise("/persistent-real", [&](const bool aSaved) { saved = aSaved; });
    });

    REQUIRE(loaded == false);
    REQUIRE(saved == false);
    REQUIRE_FALSE(storage::persistent_directory::is_loaded("/persistent-real"));
}

TEST_CASE("a persistent store synchronises its mount after every write, and says how each went",
    "[jfc::storage::persistent_directory]") {
    held_syncs held;

    static int run = 0;

    const auto mount = "/persistent-store-" + std::to_string(++run);

    storage::persistent_directory::mount(mount, {});

    held.finish_next(true);

    std::vector<bool> told;

    auto store = storage::make_persistent_store(mount + "/data/program",
        [&](const bool aSucceeded) { told.push_back(aSucceeded); });

    store->save_file("saves/one.lua", bytes("1"));

    REQUIRE(held.count() == 1);

    SECTION("writes while one is under way share the next") {
        store->save_file("saves/two.lua", bytes("2"));
        store->save_file("saves/three.lua", bytes("3"));

        REQUIRE(held.count() == 1);

        held.finish_next(true);

        REQUIRE(told == std::vector<bool>{true});
        REQUIRE(held.count() == 1);

        held.finish_next(false);

        REQUIRE(told == std::vector<bool>{true, false, false});
        REQUIRE(held.count() == 0);
    }

    SECTION("moves, removals and new directories are writes") {
        held.finish_next(true);

        store->move_file("saves/one.lua", "saves/first.lua");

        REQUIRE(held.count() == 1);

        held.finish_next(true);

        store->move_directory("saves", "kept");
        held.finish_next(true);

        store->remove_file("kept/first.lua");
        held.finish_next(true);

        store->remove_directory("kept");
        held.finish_next(true);

        store->make_directory("empty");
        held.finish_next(true);

        REQUIRE(told == std::vector<bool>{true, true, true, true, true, true});
        REQUIRE_FALSE(store->contains_directory("kept"));
        REQUIRE(store->contains_directory("empty"));
    }

    SECTION("and its files are files under its root") {
        REQUIRE(text(*storage::make_filesystem_store(mount + "/data/program")->load_file("saves/one.lua")) == "1");
    }

    SECTION("a read is not a write") {
        held.finish_next(true);

        REQUIRE(store->load_file("saves/one.lua"));
        REQUIRE(store->file_info("saves/one.lua")->size == 1);
        REQUIRE(store->files("saves") == std::vector<std::string>{"one.lua"});

        REQUIRE(held.count() == 0);
    }
}

TEST_CASE("a persistent store refuses to write before its mount has loaded", "[jfc::storage::persistent_directory]") {
    held_syncs held;

    storage::persistent_directory::mount("/persistent-early", {});

    auto store = storage::make_persistent_store("/persistent-early/data");

    REQUIRE_THROWS_AS(store->save_file("too-soon", bytes("x")), storage::exception);
    REQUIRE_FALSE(store->contains_file("too-soon"));
    REQUIRE(held.count() == 1);

    held.finish_next(true);

    REQUIRE_NOTHROW(store->save_file("in-time", bytes("x")));
    REQUIRE(held.count() == 1);

    held.finish_next(true);
}

TEST_CASE("a persistent store must be in a mounted directory", "[jfc::storage::persistent_directory]") {
    held_syncs held;

    storage::persistent_directory::mount("/persistent-held/", {});

    held.finish_next(true);

    REQUIRE_NOTHROW(storage::make_persistent_store("/persistent-held/data"));
    REQUIRE_NOTHROW(storage::make_persistent_store("/persistent-held"));
    REQUIRE_THROWS_AS(storage::make_persistent_store("/persistent-held-not"), storage::exception);
    REQUIRE_THROWS_AS(storage::make_persistent_store("/elsewhere"), storage::exception);
}
