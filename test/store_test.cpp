// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>
#include <jfc/types.h>

#include <jfc/storage/exception.h>
#include <jfc/storage/store.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace jfc::storage;

namespace {
    [[nodiscard]] std::vector<std::byte> bytes(const std::string &aText) {
        std::vector<std::byte> out;
        for (const char c : aText) out.push_back(static_cast<std::byte>(c));
        return out;
    }

    [[nodiscard]] std::string text(const std::vector<std::byte> &aBytes) {
        std::string out;
        for (const std::byte b : aBytes) out.push_back(static_cast<char>(b));
        return out;
    }

    [[nodiscard]] std::filesystem::path scratch() {
        auto path = std::filesystem::temp_directory_path()
            / ("jfc-storage-test-" + std::to_string(::rand()));
        std::filesystem::remove_all(path);
        return path;
    }

#ifndef _WIN32
    [[nodiscard]] std::filesystem::perms mode_of(const std::filesystem::path &aPath) {
        return std::filesystem::status(aPath).permissions() & std::filesystem::perms::mask;
    }
#endif
}

TEST_CASE("a store round trips a file", "[jfc::storage::store]") {
    const auto root = scratch();
    auto store = store::make_from_root(root);

    store.save_file("a/b/thing.json", bytes(R"({"token":"secret"})"));

    const auto loaded = store.load_file("a/b/thing.json");

    REQUIRE(loaded);
    REQUIRE(text(*loaded) == R"({"token":"secret"})");

    std::filesystem::remove_all(root);
}

TEST_CASE("a path cannot escape the storage root", "[jfc::storage::store]") {
    const auto root = scratch();
    auto store = store::make_from_root(root);

    REQUIRE_THROWS_AS(store.save_file("../escaped", bytes("x")), jfc::storage::exception);
    REQUIRE_THROWS_AS(store.load_file("../../etc/passwd"), jfc::storage::exception);

    std::filesystem::remove_all(root);
}

#ifndef _WIN32
TEST_CASE("a config store writes files only its owner can read", "[jfc::storage::store]") {
    const auto root = scratch();

    const auto previous = ::getenv("XDG_CONFIG_HOME");
    const std::string previousValue(previous ? previous : "");

    ::setenv("XDG_CONFIG_HOME", root.c_str(), 1);

    {
        auto store = store::make_config("some-program");

        store.save_file("conf.json", bytes(R"({"token":"secret"})"));

        const auto directory = root / "some-program";
        const auto file = directory / "conf.json";

        REQUIRE(std::filesystem::exists(file));

        INFO("directory mode " << static_cast<unsigned>(mode_of(directory)));
        REQUIRE(mode_of(directory) == std::filesystem::perms::owner_all);

        INFO("file mode " << static_cast<unsigned>(mode_of(file)));
        REQUIRE(mode_of(file) ==
            (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));

        const auto loaded = store.load_file("conf.json");
        REQUIRE(loaded);
        REQUIRE(text(*loaded) == R"({"token":"secret"})");
    }

    if (previous) ::setenv("XDG_CONFIG_HOME", previousValue.c_str(), 1);
    else ::unsetenv("XDG_CONFIG_HOME");

    std::filesystem::remove_all(root);
}

TEST_CASE("a config store tightens a file it finds already world readable",
    "[jfc::storage::store]") {

    const auto root = scratch();

    const auto previous = ::getenv("XDG_CONFIG_HOME");
    const std::string previousValue(previous ? previous : "");

    ::setenv("XDG_CONFIG_HOME", root.c_str(), 1);

    {
        const auto directory = root / "some-program";
        const auto file = directory / "conf.json";

        std::filesystem::create_directories(directory);

        {
            std::ofstream stale(file);
            stale << R"({"token":"secret"})";
        }

        std::filesystem::permissions(file,
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
            std::filesystem::perms::group_read | std::filesystem::perms::others_read,
            std::filesystem::perm_options::replace);

        REQUIRE(mode_of(file) != (std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_write));

        auto store = store::make_config("some-program");

        const auto loaded = store.load_file("conf.json");

        REQUIRE(loaded);
        REQUIRE(text(*loaded) == R"({"token":"secret"})");

        INFO("mode after the read: " << static_cast<unsigned>(mode_of(file)));
        REQUIRE(mode_of(file) ==
            (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
    }

    if (previous) ::setenv("XDG_CONFIG_HOME", previousValue.c_str(), 1);
    else ::unsetenv("XDG_CONFIG_HOME");

    std::filesystem::remove_all(root);
}

TEST_CASE("a plain store does not restrict permissions", "[jfc::storage::store]") {
    const auto root = scratch();
    auto store = store::make_from_root(root);

    store.save_file("asset.bin", bytes("data"));

    REQUIRE(mode_of(root / "asset.bin") != std::filesystem::perms::owner_read);

    std::filesystem::remove_all(root);
}
#endif


TEST_CASE("a store says what is in a directory", "[jfc::storage::store]") {
    const auto root = std::filesystem::temp_directory_path() / "jfc_storage_listing_test";

    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    auto store = jfc::storage::store::make_from_root(root);

    const std::vector<std::byte> something{std::byte{7}};

    store.save_file("worlds/vale/world.lua", something);
    store.save_file("worlds/vale/1.0.0.chunk", something);
    store.save_file("worlds/vale/0.0.0.chunk", something);
    store.save_file("worlds/vale/deeper/2.0.0.chunk", something);
    store.save_file("worlds/other/world.lua", something);

    SECTION("the files directly in it, sorted, and not those below it") {
        REQUIRE(store.files("worlds/vale")
            == std::vector<std::string>{"0.0.0.chunk", "1.0.0.chunk", "world.lua"});
    }

    SECTION("the directories directly in it, which is how the worlds are found") {
        REQUIRE(store.directories("worlds") == std::vector<std::string>{"other", "vale"});

        REQUIRE(store.files("worlds").empty());
        REQUIRE(store.directories("worlds/vale") == std::vector<std::string>{"deeper"});
    }

    SECTION("a directory that is not there is no files, not an error") {
        REQUIRE(store.files("worlds/nowhere").empty());
        REQUIRE(store.directories("worlds/nowhere").empty());
        REQUIRE(store.files("worlds/vale/world.lua").empty());
    }

    SECTION("and it cannot be asked about anywhere but the store") {
        REQUIRE_THROWS_AS(store.files("../.."), jfc::storage::exception);
        REQUIRE_THROWS_AS(store.directories("/etc"), jfc::storage::exception);
    }

    SECTION("a file removed is gone from the listing") {
        store.remove_file("worlds/vale/0.0.0.chunk");

        REQUIRE(store.files("worlds/vale") == std::vector<std::string>{"1.0.0.chunk", "world.lua"});
    }

    std::filesystem::remove_all(root);
}
