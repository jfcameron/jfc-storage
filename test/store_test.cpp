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

TEST_CASE("a plain store does not restrict permissions", "[jfc::storage::store]") {
    const auto root = scratch();
    auto store = store::make_from_root(root);

    store.save_file("asset.bin", bytes("data"));

    REQUIRE(mode_of(root / "asset.bin") != std::filesystem::perms::owner_read);

    std::filesystem::remove_all(root);
}
#endif
