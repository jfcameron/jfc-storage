// © Joseph Cameron - All Rights Reserved

#include "test_include.h"

#include <jfc/catch.hpp>

#include <jfc/storage/exception.h>
#include <jfc/storage/null_store.h>
#include <jfc/storage/store.h>

using namespace jfc;
using namespace jfc_storage_test;

TEST_CASE("a null store keeps nothing", "[jfc::storage::null_store]") {
    auto store = storage::make_null_store();

    store->save_file("worlds/vale/world.lua", bytes("return {}"));

    REQUIRE_FALSE(store->load_file("worlds/vale/world.lua"));
    REQUIRE(store->files("worlds/vale").empty());
    REQUIRE(store->directories("worlds").empty());
    REQUIRE(store->directories("").empty());
}

TEST_CASE("a null store has nothing to move, and removes nothing", "[jfc::storage::null_store]") {
    auto store = storage::make_null_store();

    REQUIRE_FALSE(store->contains_file("a"));
    REQUIRE_FALSE(store->contains_directory("a"));
    REQUIRE(store->contains_directory(""));

    REQUIRE_THROWS_AS(store->move_file("a", "b"), storage::exception);
    REQUIRE_THROWS_AS(store->move_directory("a", "b"), storage::exception);

    REQUIRE_NOTHROW(store->remove_file("a"));
    REQUIRE_NOTHROW(store->remove_directory("a"));
}
