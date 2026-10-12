// © Joseph Cameron - All Rights Reserved

#include "test_include.h"

#include <jfc/catch.hpp>

#include <jfc/storage/exception.h>
#include <jfc/storage/memory_store.h>
#include <jfc/storage/null_store.h>
#include <jfc/storage/path.h>
#include <jfc/storage/store.h>

#include <string>

using namespace jfc;
using namespace jfc_storage_test;

namespace {
    void require_refused(const std::string &aPath, const std::string &aWhy, const std::string &aShownAs) {
        auto store = storage::make_null_store();

        INFO("path \"" << aShownAs << "\"");

        REQUIRE(storage::path_refusal(aPath) == aWhy);

        REQUIRE_THROWS_MATCHES(store->save_file(aPath, bytes("x")), storage::path_refused,
            Catch::Matchers::Message("jfc::storage: cannot save \"" + aShownAs + "\": " + aWhy));

        REQUIRE_THROWS_AS(store->load_file(aPath), storage::path_refused);
        REQUIRE_THROWS_AS(store->contains_file(aPath), storage::path_refused);
        REQUIRE_THROWS_AS(store->contains_directory(aPath), storage::path_refused);
        REQUIRE_THROWS_AS(store->files(aPath), storage::path_refused);
        REQUIRE_THROWS_AS(store->directories(aPath), storage::path_refused);
        REQUIRE_THROWS_AS(store->remove_file(aPath), storage::path_refused);
        REQUIRE_THROWS_AS(store->remove_directory(aPath), storage::path_refused);
        REQUIRE_THROWS_AS(store->move_file(aPath, "fine"), storage::path_refused);
        REQUIRE_THROWS_AS(store->move_file("fine", aPath), storage::path_refused);
        REQUIRE_THROWS_AS(store->move_directory(aPath, "fine"), storage::path_refused);
        REQUIRE_THROWS_AS(store->move_directory("fine", aPath), storage::path_refused);
    }

    void require_refused(const std::string &aPath, const std::string &aWhy) {
        require_refused(aPath, aWhy, aPath);
    }

    void require_accepted(const std::string &aPath) {
        auto store = storage::make_memory_store();

        INFO("path \"" << aPath << "\"");

        REQUIRE_FALSE(storage::path_refusal(aPath));

        REQUIRE_NOTHROW(store->save_file(aPath, bytes("x")));
        REQUIRE(store->contains_file(aPath));
        REQUIRE(store->load_file(aPath));
        REQUIRE_NOTHROW(store->move_file(aPath, aPath));
        REQUIRE_NOTHROW(store->files(aPath));
        REQUIRE_NOTHROW(store->directories(aPath));
        REQUIRE_NOTHROW(store->remove_directory(aPath));
        REQUIRE_NOTHROW(store->remove_file(aPath));
        REQUIRE_FALSE(store->contains_file(aPath));
    }
}

TEST_CASE("a path may not leave the store", "[jfc::storage::store]") {
    require_refused("/etc/passwd", "it is absolute; paths are relative to the store");
    require_refused("../escaped", "it climbs above the store's root");
    require_refused("a/../../escaped", "it climbs above the store's root");
    require_refused("..", "it climbs above the store's root");
}

TEST_CASE("a path may wander as long as it stays in the store", "[jfc::storage::store]") {
    require_accepted("a/../b");
    require_accepted("./a/./b");
    require_accepted("a//b/");
    require_accepted("worlds/vale/0.0.0.chunk");
    require_accepted("-1.-2.3.chunk");
    require_accepted(".hidden");
    require_accepted("a..b");
    require_accepted("console.lua");
    require_accepted("com10");
}

TEST_CASE("a path means the same on every system", "[jfc::storage::store]") {
    require_refused("a\\b", "it contains '\\'; the separator is '/'");
    require_refused("c:thing", "it contains ':', which windows reads as a drive or a stream");
    require_refused("file:stream", "it contains ':', which windows reads as a drive or a stream");

    for (const std::string each : {"*", "?", "\"", "<", ">", "|"})
        require_refused("a" + each + "b",
            "it contains a character windows does not allow in a name: * ? \" < > |");

    require_refused("a\tb", "it contains a control character", "a\\x09b");
    require_refused(std::string("a\0b", 3), "it contains a control character", "a\\x00b");
    require_refused("a\x7F" "b", "it contains a control character", "a\\x7fb");

    require_refused("saves/world.", "a name ends in '.' or ' ', which windows removes");
    require_refused("saves /world", "a name ends in '.' or ' ', which windows removes");

    for (const std::string each : {"con", "CON", "prn", "aux", "nul", "Nul.txt", "com1", "lpt9.tar.gz"})
        require_refused("saves/" + each, "a name is one windows reserves for a device");
}

TEST_CASE("a store keeps names beginning \".jfc-storage\" for its own use", "[jfc::storage::store]") {
    for (const std::string each : {".jfc-storage", ".jfc-storage-saving-world.lua", ".JFC-Storage-x"})
        require_refused("saves/" + each, "a name begins \".jfc-storage\", which stores keep for their own use");

    require_accepted("jfc-storage");
    require_accepted("saves/.jfc-storag");
    require_accepted("saves/x.jfc-storage");
}

TEST_CASE("a path is UTF-8", "[jfc::storage::store]") {
    require_accepted("Caf\xC3\xA9");
    require_accepted("\xE6\x97\xA5\xE6\x9C\xAC/\xF0\x9F\x8C\x8D");

    require_refused("\xFF", "it is not UTF-8");
    require_refused("Caf\xC3", "it is not UTF-8");
    require_refused("\xC0\xAF", "it is not UTF-8");
    require_refused("\xED\xA0\x80", "it is not UTF-8");
    require_refused("\xF4\x90\x80\x80", "it is not UTF-8");
}

TEST_CASE("a file operation must name a file", "[jfc::storage::store]") {
    auto store = storage::make_null_store();

    for (const std::string each : {"", ".", "a/.."}) {
        INFO("path \"" << each << "\"");

        REQUIRE_THROWS_AS(store->save_file(each, bytes("x")), storage::exception);
        REQUIRE_THROWS_AS(store->load_file(each), storage::exception);
        REQUIRE_THROWS_AS(store->remove_file(each), storage::exception);
    }

    REQUIRE_THROWS_MATCHES(store->load_file(""), storage::exception,
        Catch::Matchers::Message("jfc::storage: cannot load \"\": it names the store's root, not a file"));

    SECTION("but a listing may name the root") {
        REQUIRE_NOTHROW(store->files(""));
        REQUIRE_NOTHROW(store->directories("."));
    }
}

TEST_CASE("the root cannot be removed", "[jfc::storage::store]") {
    auto store = storage::make_null_store();

    for (const std::string each : {"", ".", "a/.."}) {
        INFO("path \"" << each << "\"");

        REQUIRE_THROWS_MATCHES(store->remove_directory(each), storage::exception,
            Catch::Matchers::Message("jfc::storage: cannot remove the directory \"" + each
                + "\": it is the store's root"));
    }
}

TEST_CASE("a refused move names the side that was refused", "[jfc::storage::store]") {
    auto store = storage::make_null_store();

    REQUIRE_THROWS_MATCHES(store->move_file("../a", "b"), storage::exception,
        Catch::Matchers::Message("jfc::storage: cannot move from \"../a\": it climbs above the store's root"));

    REQUIRE_THROWS_MATCHES(store->move_file("a", "../b"), storage::exception,
        Catch::Matchers::Message("jfc::storage: cannot move to \"../b\": it climbs above the store's root"));
}
