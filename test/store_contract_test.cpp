// © Joseph Cameron - MIT License

#include "test_include.h"

#include <jfc/catch.hpp>

#include <jfc/storage/exception.h>
#include <jfc/storage/ext/copy.h>
#include <jfc/storage/filesystem_store.h>
#include <jfc/storage/memory_store.h>
#include <jfc/storage/store.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

using namespace jfc;
using namespace jfc_storage_test;

namespace {
    struct in_memory final {
        storage::store_ptr_type store = storage::make_memory_store();
    };

    struct on_disk final {
        scratch_directory root{"contract"};

        storage::store_ptr_type store = storage::make_filesystem_store(root.path());
    };

    struct on_disk_flushed final {
        scratch_directory root{"contract_flushed"};

        storage::store_ptr_type store = storage::make_filesystem_store(root.path(), {.flush_to_disk = true});
    };

    using names = std::vector<std::string>;
}

#define JFC_STORAGE_CONTRACT(aName) \
    TEMPLATE_TEST_CASE(aName, "[jfc::storage::store][contract]", in_memory, on_disk, on_disk_flushed)

JFC_STORAGE_CONTRACT("a store gives back what was saved") {
    TestType fixture;
    auto &store = *fixture.store;

    store.save_file("a/b/thing.json", bytes(R"({"token":"secret"})"));

    REQUIRE(text(*store.load_file("a/b/thing.json")) == R"({"token":"secret"})");
    REQUIRE(store.contains_file("a/b/thing.json"));

    SECTION("by any path that resolves to the same place") {
        REQUIRE(text(*store.load_file("a/./x/../b//thing.json")) == R"({"token":"secret"})");
    }

    SECTION("and a save replaces what was there") {
        store.save_file("a/b/thing.json", bytes("shorter"));

        REQUIRE(text(*store.load_file("a/b/thing.json")) == "shorter");
        REQUIRE(store.files("a/b") == names{"thing.json"});
    }

    SECTION("and an empty file is a file, not an absence") {
        store.save_file("empty", {});

        REQUIRE(store.load_file("empty"));
        REQUIRE(store.load_file("empty")->empty());
        REQUIRE(store.contains_file("empty"));
    }

    SECTION("and names are UTF-8") {
        const std::string world = "\xE6\x97\xA5\xE6\x9C\xAC";
        const std::string cafe = "Caf\xC3\xA9.lua";

        store.save_file(world + "/" + cafe, bytes("x"));

        REQUIRE(store.files(world) == names{cafe});
        REQUIRE(store.directories("") == names{"a", world});
    }
}

namespace {
    [[nodiscard]] long long ticks_of(const std::filesystem::file_time_type aTime) {
        return static_cast<long long>(aTime.time_since_epoch().count());
    }
}

JFC_STORAGE_CONTRACT("a store says how big a file is and when it was written") {
    TestType fixture;
    auto &store = *fixture.store;

    using clock = std::filesystem::file_time_type::clock;

    const auto before = clock::now() - std::chrono::seconds(2);

    store.save_file("a/thing", bytes("12345"));

    const auto after = clock::now() + std::chrono::seconds(2);

    const auto info = store.file_info("a/thing");

    REQUIRE(info);
    REQUIRE(info->size == 5);
    REQUIRE(ticks_of(info->modified) >= ticks_of(before));
    REQUIRE(ticks_of(info->modified) <= ticks_of(after));

    SECTION("the same, every time it is asked") {
        REQUIRE(ticks_of(store.file_info("a/thing")->modified) == ticks_of(info->modified));
    }

    SECTION("and a move keeps it") {
        store.move_file("a/thing", "b/moved");

        REQUIRE(store.file_info("b/moved")->size == 5);
        REQUIRE(ticks_of(store.file_info("b/moved")->modified) == ticks_of(info->modified));

        store.move_directory("b", "c");

        REQUIRE(ticks_of(store.file_info("c/moved")->modified) == ticks_of(info->modified));
    }

    SECTION("and a save writes it again") {
        store.save_file("a/thing", bytes("12"));

        REQUIRE(store.file_info("a/thing")->size == 2);
        REQUIRE(ticks_of(store.file_info("a/thing")->modified) >= ticks_of(info->modified));
    }

    SECTION("an empty file is empty") {
        store.save_file("empty", {});

        REQUIRE(store.file_info("empty")->size == 0);
    }

    SECTION("and there is nothing to say of what is not a file") {
        REQUIRE_FALSE(store.file_info("a"));
        REQUIRE_FALSE(store.file_info("nowhere"));
        REQUIRE_FALSE(store.file_info("a/thing/beneath"));
        REQUIRE_THROWS_AS(store.file_info(""), storage::path_refused);
    }
}

JFC_STORAGE_CONTRACT("a store has nothing where nothing was saved") {
    TestType fixture;
    auto &store = *fixture.store;

    store.save_file("a/thing", bytes("x"));

    REQUIRE_FALSE(store.load_file("nowhere"));
    REQUIRE_FALSE(store.load_file("no/where/at/all"));
    REQUIRE_FALSE(store.contains_file("nowhere"));
    REQUIRE_FALSE(store.contains_directory("nowhere"));

    SECTION("a directory is not a file") {
        REQUIRE_FALSE(store.load_file("a"));
        REQUIRE_FALSE(store.contains_file("a"));
        REQUIRE(store.contains_directory("a"));
    }

    SECTION("a file is not a directory") {
        REQUIRE_FALSE(store.contains_directory("a/thing"));
        REQUIRE(store.files("a/thing").empty());
        REQUIRE_FALSE(store.load_file("a/thing/beneath"));
    }

    SECTION("the root is always a directory") {
        REQUIRE(store.contains_directory(""));
        REQUIRE(store.contains_directory("."));
    }
}

JFC_STORAGE_CONTRACT("a store says what is in a directory") {
    TestType fixture;
    auto &store = *fixture.store;

    const std::vector<std::byte> something{std::byte{7}};

    store.save_file("worlds/vale/world.lua", something);
    store.save_file("worlds/vale/1.0.0.chunk", something);
    store.save_file("worlds/vale/0.0.0.chunk", something);
    store.save_file("worlds/vale/deeper/2.0.0.chunk", something);
    store.save_file("worlds/vale-two/world.lua", something);
    store.save_file("worlds/other/world.lua", something);

    SECTION("the files directly in it, in byte order, and not those below it") {
        REQUIRE(store.files("worlds/vale") == names{"0.0.0.chunk", "1.0.0.chunk", "world.lua"});
    }

    SECTION("the directories directly in it, in byte order") {
        REQUIRE(store.directories("worlds") == names{"other", "vale", "vale-two"});
        REQUIRE(store.directories("worlds/vale") == names{"deeper"});
        REQUIRE(store.directories("") == names{"worlds"});
        REQUIRE(store.files("worlds").empty());
    }

    SECTION("a directory that is not there has nothing in it") {
        REQUIRE(store.files("worlds/nowhere").empty());
        REQUIRE(store.directories("worlds/nowhere").empty());
    }

    SECTION("a directory stays when its files are removed") {
        store.remove_file("worlds/other/world.lua");

        REQUIRE(store.files("worlds/other").empty());
        REQUIRE(store.directories("worlds") == names{"other", "vale", "vale-two"});
        REQUIRE(store.contains_directory("worlds/other"));
    }
}

JFC_STORAGE_CONTRACT("a save cannot put a file where a directory is, or under a file") {
    TestType fixture;
    auto &store = *fixture.store;

    store.save_file("a/b", bytes("x"));

    REQUIRE_THROWS_AS(store.save_file("a", bytes("y")), storage::exception);
    REQUIRE_THROWS_AS(store.save_file("a/b/c", bytes("y")), storage::exception);

    try { store.save_file("a/b/c", bytes("y")); }
    catch (const storage::path_refused &) { FAIL("a path every store accepts was refused as a path"); }
    catch (const storage::exception &) {}

    REQUIRE(text(*store.load_file("a/b")) == "x");
}

JFC_STORAGE_CONTRACT("a store makes directories") {
    TestType fixture;
    auto &store = *fixture.store;

    store.make_directory("saves/slot one");

    REQUIRE(store.contains_directory("saves/slot one"));
    REQUIRE(store.directories("") == names{"saves"});
    REQUIRE(store.directories("saves") == names{"slot one"});
    REQUIRE(store.files("saves/slot one").empty());

    SECTION("and one already there is left as it is") {
        store.save_file("saves/slot one/player.lua", bytes("x"));

        store.make_directory("saves/slot one");
        store.make_directory("./saves/");

        REQUIRE(text(*store.load_file("saves/slot one/player.lua")) == "x");
    }

    SECTION("and the root is always there") {
        REQUIRE_NOTHROW(store.make_directory(""));
        REQUIRE(store.directories("") == names{"saves"});
    }

    SECTION("and a file can be saved in one") {
        store.save_file("saves/slot one/player.lua", bytes("x"));

        REQUIRE(store.files("saves/slot one") == names{"player.lua"});
    }

    SECTION("but not where a file is, nor under one") {
        store.save_file("saves/notes", bytes("x"));

        REQUIRE_THROWS_AS(store.make_directory("saves/notes"), storage::exception);
        REQUIRE_THROWS_AS(store.make_directory("saves/notes/deeper"), storage::exception);

        try { store.make_directory("saves/notes/deeper"); }
        catch (const storage::path_refused &) { FAIL("a path every store accepts was refused as a path"); }
        catch (const storage::exception &) {}

        REQUIRE(text(*store.load_file("saves/notes")) == "x");
        REQUIRE_FALSE(store.contains_directory("saves/notes"));
    }

    SECTION("nor outside the store") {
        REQUIRE_THROWS_AS(store.make_directory("../saves"), storage::path_refused);
    }
}

JFC_STORAGE_CONTRACT("a store removes files and directories") {
    TestType fixture;
    auto &store = *fixture.store;

    store.save_file("worlds/vale/world.lua", bytes("x"));
    store.save_file("worlds/vale/deeper/0.0.0.chunk", bytes("x"));
    store.save_file("worlds/other/world.lua", bytes("x"));

    SECTION("a file") {
        store.remove_file("worlds/vale/world.lua");

        REQUIRE_FALSE(store.contains_file("worlds/vale/world.lua"));
        REQUIRE(store.contains_file("worlds/vale/deeper/0.0.0.chunk"));
    }

    SECTION("a directory and everything in it") {
        store.remove_directory("worlds/vale");

        REQUIRE(store.directories("worlds") == names{"other"});
        REQUIRE_FALSE(store.contains_directory("worlds/vale"));
        REQUIRE_FALSE(store.contains_directory("worlds/vale/deeper"));
        REQUIRE_FALSE(store.contains_file("worlds/vale/deeper/0.0.0.chunk"));
        REQUIRE(store.contains_file("worlds/other/world.lua"));
    }

    SECTION("nothing, for what is not there") {
        REQUIRE_NOTHROW(store.remove_file("nowhere"));
        REQUIRE_NOTHROW(store.remove_directory("nowhere/at/all"));
    }

    SECTION("nothing, for a directory asked to be removed as a file, or a file as a directory") {
        store.remove_file("worlds/vale");
        store.remove_directory("worlds/other/world.lua");

        REQUIRE(store.contains_directory("worlds/vale"));
        REQUIRE(store.contains_file("worlds/other/world.lua"));
    }
}

JFC_STORAGE_CONTRACT("a store moves files") {
    TestType fixture;
    auto &store = *fixture.store;

    store.save_file("from", bytes("moved"));

    SECTION("into a directory it makes") {
        store.move_file("from", "a/b/to");

        REQUIRE_FALSE(store.contains_file("from"));
        REQUIRE(text(*store.load_file("a/b/to")) == "moved");
    }

    SECTION("over a file already there") {
        store.save_file("to", bytes("replaced"));

        store.move_file("from", "to");

        REQUIRE(text(*store.load_file("to")) == "moved");
        REQUIRE(store.files("") == names{"to"});
    }

    SECTION("onto itself, which changes nothing") {
        store.move_file("from", "./from");

        REQUIRE(text(*store.load_file("from")) == "moved");
    }

    SECTION("but not from a file that is not there") {
        REQUIRE_THROWS_MATCHES(store.move_file("nowhere", "to"), storage::not_found,
            Catch::Matchers::Message("jfc::storage: cannot move \"nowhere\": there is no file there"));
    }

    SECTION("nor onto a directory") {
        store.save_file("directory/inside", bytes("x"));

        REQUIRE_THROWS_MATCHES(store.move_file("from", "directory"), storage::exception,
            Catch::Matchers::Message("jfc::storage: cannot move \"from\": a directory is at \"directory\""));

        REQUIRE(store.contains_file("from"));
    }
}

JFC_STORAGE_CONTRACT("a store moves directories") {
    TestType fixture;
    auto &store = *fixture.store;

    store.save_file("players/ann/base.lua", bytes("ann"));
    store.save_file("players/ann/1/base.lua", bytes("first"));
    store.save_file("players/bob/base.lua", bytes("bob"));

    SECTION("and everything in them") {
        store.move_directory("players/ann", "players/anne");

        REQUIRE(store.directories("players") == names{"anne", "bob"});
        REQUIRE(text(*store.load_file("players/anne/base.lua")) == "ann");
        REQUIRE(text(*store.load_file("players/anne/1/base.lua")) == "first");
        REQUIRE_FALSE(store.contains_directory("players/ann"));
    }

    SECTION("into a directory it makes") {
        store.move_directory("players", "saves/worlds/vale/players");

        REQUIRE(store.directories("") == names{"saves"});
        REQUIRE(text(*store.load_file("saves/worlds/vale/players/bob/base.lua")) == "bob");
    }

    SECTION("but not from a directory that is not there") {
        REQUIRE_THROWS_MATCHES(store.move_directory("nowhere", "somewhere"), storage::not_found,
            Catch::Matchers::Message("jfc::storage: cannot move the directory \"nowhere\": there is no directory "
                "there"));
    }

    SECTION("nor onto anything already there") {
        store.save_file("taken", bytes("x"));

        REQUIRE_THROWS_AS(store.move_directory("players/ann", "players/bob"), storage::exception);
        REQUIRE_THROWS_AS(store.move_directory("players/ann", "taken"), storage::exception);

        try { store.move_directory("players/ann", "players/bob"); }
        catch (const storage::not_found &) { FAIL("something in the way was reported as nothing there"); }
        catch (const storage::exception &) {}

        REQUIRE(text(*store.load_file("players/bob/base.lua")) == "bob");
        REQUIRE(store.contains_directory("players/ann"));
    }

    SECTION("nor into itself, nor the root") {
        REQUIRE_THROWS_AS(store.move_directory("players", "players/ann/players"), storage::path_refused);
        REQUIRE_THROWS_AS(store.move_directory("players", "players"), storage::path_refused);
        REQUIRE_THROWS_AS(store.move_directory("", "elsewhere"), storage::path_refused);
        REQUIRE_THROWS_AS(store.move_directory("players", "."), storage::path_refused);
    }
}

JFC_STORAGE_CONTRACT("a store copies, through what every store offers") {
    TestType fixture;
    auto &store = *fixture.store;

    store.save_file("worlds/vale/world.lua", bytes("vale"));
    store.save_file("worlds/vale/deeper/0.0.0.chunk", bytes("chunk"));

    SECTION("a file") {
        storage::ext::copy_file(store, "worlds/vale/world.lua", store, "copied.lua");

        REQUIRE(text(*store.load_file("copied.lua")) == "vale");
        REQUIRE(store.contains_file("worlds/vale/world.lua"));
    }

    SECTION("a directory, within the store") {
        storage::ext::copy_directory(store, "worlds/vale", store, "worlds/vale copy");

        REQUIRE(store.directories("worlds") == names{"vale", "vale copy"});
        REQUIRE(text(*store.load_file("worlds/vale copy/deeper/0.0.0.chunk")) == "chunk");
    }

    SECTION("a directory, into itself, as it was when the copy began") {
        storage::ext::copy_directory(store, "worlds/vale", store, "worlds/vale/again");

        REQUIRE(text(*store.load_file("worlds/vale/again/world.lua")) == "vale");
        REQUIRE_FALSE(store.contains_directory("worlds/vale/again/again"));
    }

    SECTION("a directory, with the empty directories in it") {
        store.make_directory("worlds/vale/players");
        store.make_directory("empty");

        storage::ext::copy_directory(store, "worlds/vale", store, "worlds/vale copy");
        storage::ext::copy_directory(store, "empty", store, "still empty");

        REQUIRE(store.directories("worlds/vale copy") == names{"deeper", "players"});
        REQUIRE(store.contains_directory("still empty"));
    }

    SECTION("a directory, into another kind of store") {
        auto other = storage::make_memory_store();

        storage::ext::copy_directory(store, "worlds", *other, "");

        REQUIRE(text(*other->load_file("vale/deeper/0.0.0.chunk")) == "chunk");
    }

    SECTION("but not what is not there, nor over what is") {
        REQUIRE_THROWS_AS(storage::ext::copy_file(store, "nowhere", store, "to"), storage::not_found);
        REQUIRE_THROWS_AS(storage::ext::copy_directory(store, "nowhere", store, "to"), storage::not_found);
        REQUIRE_THROWS_AS(storage::ext::copy_directory(store, "worlds/vale", store, "worlds"), storage::exception);
    }
}
