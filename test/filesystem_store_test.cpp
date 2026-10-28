// © Joseph Cameron - MIT License

#include "test_include.h"

#include <jfc/catch.hpp>

#include <jfc/storage/exception.h>
#include <jfc/storage/ext/copy.h>
#include <jfc/storage/filesystem_store.h>
#include <jfc/storage/store.h>

#include <algorithm>
#include <atomic>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

using namespace jfc;
using namespace jfc_storage_test;

TEST_CASE("a filesystem store's files are files under its root", "[jfc::storage::filesystem_store]") {
    const scratch_directory root("on_disk");

    auto store = storage::make_filesystem_store(root.path());

    const std::string world = "\xE6\x97\xA5\xE6\x9C\xAC";
    const std::string cafe = "Caf\xC3\xA9.lua";

    store->save_file("a/b/thing.json", bytes("x"));
    store->save_file(world + "/" + cafe, bytes("x"));

    REQUIRE(std::filesystem::is_regular_file(root.path() / "a" / "b" / "thing.json"));
    REQUIRE(std::filesystem::is_regular_file(root.path() / utf8_path(world) / utf8_path(cafe)));

    SECTION("and a directory moved is moved on disk") {
        store->move_directory("a", "c/d");

        REQUIRE(std::filesystem::is_regular_file(root.path() / "c" / "d" / "b" / "thing.json"));
        REQUIRE_FALSE(std::filesystem::exists(root.path() / "a"));
    }
}

TEST_CASE("a filesystem store lists only what a path can name", "[jfc::storage::filesystem_store]") {
    const scratch_directory root("unnameable");

    auto store = storage::make_filesystem_store(root.path());

    store->save_file("worlds/vale/world.lua", bytes("x"));

    const auto vale = root.path() / "worlds" / "vale";

    SECTION("so a save that never finished is not listed") {
        std::ofstream(vale / ".jfc-storage-saving-0123456789abcdef-0000000000000001") << "half";

        REQUIRE(store->files("worlds/vale") == std::vector<std::string>{"world.lua"});

        SECTION("and the next save of that file is not disturbed by it") {
            store->save_file("worlds/vale/world.lua", bytes("y"));

            REQUIRE(text(*store->load_file("worlds/vale/world.lua")) == "y");
        }
    }

    SECTION("so a name put there from outside, that no path can name, is not listed") {
        std::ofstream(vale / ".JFC-Storage-elsewhere") << "x";
        std::filesystem::create_directories(root.path() / "worlds" / ".jfc-storage");

#ifndef _WIN32
        std::ofstream(vale / "nul.txt") << "x";
        std::ofstream(vale / "colon:name") << "x";
        std::filesystem::create_directories(root.path() / "worlds" / "trailing.");
#endif

        REQUIRE(store->files("worlds/vale") == std::vector<std::string>{"world.lua"});
        REQUIRE(store->directories("worlds") == std::vector<std::string>{"vale"});
    }
}

TEST_CASE("two stores saving one file at once each save it whole", "[jfc::storage::filesystem_store]") {
#ifdef __EMSCRIPTEN__
    WARN("a page has no threads: nothing to check");
#else
    const scratch_directory root("two_savers");

    std::atomic<int> failures = 0;

    const auto saver = [&](const char aFill) {
        auto store = storage::make_filesystem_store(root.path());

        const std::vector<std::byte> data(64 * 1024, static_cast<std::byte>(aFill));

        for (int i = 0; i < 100; ++i) {
            try { store->save_file("settings.lua", data); }
            catch (const std::exception &) { ++failures; }
        }
    };

    {
        std::thread first(saver, 'a');
        std::thread second(saver, 'b');

        first.join();
        second.join();
    }

    REQUIRE(failures == 0);

    const auto saved = *storage::make_filesystem_store(root.path())->load_file("settings.lua");

    REQUIRE(saved.size() == 64 * 1024);
    REQUIRE(std::all_of(saved.begin(), saved.end(), [&](const std::byte aEach) { return aEach == saved.front(); }));

    REQUIRE(std::distance(std::filesystem::directory_iterator(root.path()), std::filesystem::directory_iterator()) == 1);
#endif
}

TEST_CASE("a file may have as long a name as the system allows", "[jfc::storage::filesystem_store]") {
#ifdef _WIN32
    WARN("windows' MAX_PATH is reached before a name's limit: nothing to check");
#else
    const scratch_directory root("long_name");

    auto store = storage::make_filesystem_store(root.path());

    const std::string name(255, 'n');

    store->save_file(name, bytes("x"));

    REQUIRE(text(*store->load_file(name)) == "x");
#endif
}

TEST_CASE("a store flushed to disk can be restricted to its owner too", "[jfc::storage::filesystem_store]") {
    const scratch_directory root("flushed_restricted");

    auto store = storage::make_filesystem_store(root.path() / "both",
        {.restrict_to_owner = true, .flush_to_disk = true});

    store->save_file("conf.json", bytes("secret"));

    REQUIRE(text(*store->load_file("conf.json")) == "secret");
}

TEST_CASE("a filesystem store keeps the root it was made with", "[jfc::storage::filesystem_store]") {
    const scratch_directory root("relative_root");

    const auto previous = std::filesystem::current_path();

    std::filesystem::current_path(root.path());

    auto store = storage::make_filesystem_store("here");

    std::filesystem::current_path(previous);

    store->save_file("thing", bytes("x"));

    REQUIRE(std::filesystem::is_regular_file(root.path() / "here" / "thing"));

    SECTION("and an empty root is no root") {
        REQUIRE_THROWS_AS(storage::make_filesystem_store(""), storage::exception);
    }
}

TEST_CASE("a save that fails says where", "[jfc::storage::filesystem_store]") {
    const scratch_directory root("failed_save");

    auto store = storage::make_filesystem_store(root.path());

    std::filesystem::create_directories(root.path() / "taken" / "inside");

    REQUIRE_THROWS_WITH(store->save_file("taken", bytes("x")),
        Catch::Matchers::StartsWith("jfc::storage: cannot save \"taken\" in \"")
        && Catch::Matchers::Contains("cannot replace the file"));

    REQUIRE_FALSE(std::filesystem::exists(root.path() / ".jfc-storage-saving-taken"));
}

#ifndef _WIN32
namespace {
    [[nodiscard]] std::filesystem::perms mode_of(const std::filesystem::path &aPath) {
        return std::filesystem::status(aPath).permissions() & std::filesystem::perms::mask;
    }

    constexpr auto OWNER_FILE = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write;
}

TEST_CASE("a link is listed as what it leads to", "[jfc::storage::filesystem_store]") {
    const scratch_directory scratch("listed_links");

    const auto outside = scratch.path() / "outside";
    const auto root = scratch.path() / "root";

    std::filesystem::create_directories(outside / "vale");

    std::ofstream(outside / "vale" / "world.lua") << "vale";
    std::ofstream(outside / "notes.txt") << "notes";

    auto store = storage::make_filesystem_store(root);

    store->save_file("worlds/real/world.lua", bytes("real"));

    std::filesystem::create_directory_symlink(outside / "vale", root / "worlds" / "vale");
    std::filesystem::create_symlink(outside / "notes.txt", root / "notes.txt");
    std::filesystem::create_symlink(scratch.path() / "nowhere", root / "broken");

    REQUIRE(store->directories("worlds") == std::vector<std::string>{"real", "vale"});
    REQUIRE(store->files("worlds/vale") == std::vector<std::string>{"world.lua"});
    REQUIRE(store->files("") == std::vector<std::string>{"notes.txt"});
    REQUIRE(store->directories("") == std::vector<std::string>{"worlds"});

    SECTION("but not a link back to where it is, or above it, so a copy ends") {
        std::filesystem::create_directory_symlink(root / "worlds", root / "worlds" / "real" / "up");
        std::filesystem::create_directory_symlink(root, root / "worlds" / "top");
        std::filesystem::create_directory_symlink(root / "worlds" / "real", root / "worlds" / "real" / "itself");

        REQUIRE(store->directories("worlds") == std::vector<std::string>{"real", "vale"});
        REQUIRE(store->directories("worlds/real").empty());

        storage::ext::copy_directory(*store, "worlds", *store, "copied");

        REQUIRE(text(*store->load_file("copied/vale/world.lua")) == "vale");
        REQUIRE(text(*store->load_file("copied/real/world.lua")) == "real");
    }
}

TEST_CASE("removing a link to a directory leaves what it leads to", "[jfc::storage::filesystem_store]") {
    const scratch_directory scratch("remove_link");

    const auto outside = scratch.path() / "outside";

    std::filesystem::create_directories(outside);

    {
        std::ofstream kept(outside / "kept");
        kept << "x";
    }

    auto store = storage::make_filesystem_store(scratch.path() / "root");

    store->save_file("placeholder", bytes("x"));

    std::filesystem::create_directory_symlink(outside, scratch.path() / "root" / "link");

    store->remove_directory("link");

    REQUIRE_FALSE(std::filesystem::exists(std::filesystem::symlink_status(scratch.path() / "root" / "link")));
    REQUIRE(std::filesystem::is_regular_file(outside / "kept"));
}

TEST_CASE("a save that fails leaves the file as it was", "[jfc::storage::filesystem_store]") {
    const scratch_directory root("unwritable");

    auto store = storage::make_filesystem_store(root.path());

    store->save_file("dir/kept", bytes("before"));

    std::filesystem::permissions(root.path() / "dir",
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace);

    REQUIRE_THROWS_AS(store->save_file("dir/kept", bytes("after")), storage::exception);

    std::filesystem::permissions(root.path() / "dir", std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);

    REQUIRE(text(*store->load_file("dir/kept")) == "before");
}

TEST_CASE("a flushed save that fails leaves the file as it was", "[jfc::storage::filesystem_store]") {
    const scratch_directory root("flushed_unwritable");

    auto store = storage::make_filesystem_store(root.path(), {.flush_to_disk = true});

    store->save_file("dir/kept", bytes("before"));

    std::filesystem::permissions(root.path() / "dir",
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace);

    REQUIRE_THROWS_AS(store->save_file("dir/kept", bytes("after")), storage::exception);

    std::filesystem::permissions(root.path() / "dir", std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);

    REQUIRE(text(*store->load_file("dir/kept")) == "before");
}

TEST_CASE("a store restricted to its owner writes what only its owner can read",
    "[jfc::storage::filesystem_store]") {

    const scratch_directory scratch("restricted");

    const auto root = scratch.path() / "some-program";

    auto store = storage::make_filesystem_store(root, {.restrict_to_owner = true});

    REQUIRE(mode_of(root) == std::filesystem::perms::owner_all);

    store->save_file("conf.json", bytes(R"({"token":"secret"})"));
    store->save_file("deeper/conf.json", bytes(R"({"token":"secret"})"));

    REQUIRE(mode_of(root / "conf.json") == OWNER_FILE);
    REQUIRE(mode_of(root / "deeper") == std::filesystem::perms::owner_all);
    REQUIRE(mode_of(root / "deeper" / "conf.json") == OWNER_FILE);

    REQUIRE(text(*store->load_file("conf.json")) == R"({"token":"secret"})");
}

TEST_CASE("a store restricted to its owner leaves a file it loads as it is, below a root only its owner can enter",
    "[jfc::storage::filesystem_store]") {

    const scratch_directory scratch("tightened");

    const auto file = scratch.path() / "conf.json";

    {
        std::ofstream stale(file);
        stale << R"({"token":"secret"})";
    }

    std::filesystem::permissions(file, OWNER_FILE
        | std::filesystem::perms::group_read | std::filesystem::perms::others_read,
        std::filesystem::perm_options::replace);

    REQUIRE(mode_of(file) != OWNER_FILE);

    auto store = storage::make_filesystem_store(scratch.path(), {.restrict_to_owner = true});

    REQUIRE(text(*store->load_file("conf.json")) == R"({"token":"secret"})");

    REQUIRE(mode_of(file) != OWNER_FILE);
    REQUIRE(mode_of(scratch.path()) == std::filesystem::perms::owner_all);

    SECTION("until it is saved") {
        store->save_file("conf.json", bytes("{}"));

        REQUIRE(mode_of(file) == OWNER_FILE);
    }
}

TEST_CASE("a store not restricted leaves permissions to the system", "[jfc::storage::filesystem_store]") {
    const scratch_directory root("unrestricted");

    auto store = storage::make_filesystem_store(root.path());

    store->save_file("asset.bin", bytes("data"));

    REQUIRE(mode_of(root.path() / "asset.bin") != OWNER_FILE);
}
#endif
