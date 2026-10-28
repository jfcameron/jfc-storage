// © Joseph Cameron - MIT License

#include "test_include.h"

#include <jfc/catch.hpp>

#include <jfc/storage/exception.h>
#include <jfc/storage/user_directories.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using namespace jfc;
using namespace jfc_storage_test;

TEST_CASE("a user directory is the program's own, under the system's", "[jfc::storage::user_directories]") {
    for (const auto &directory : {storage::user_data_directory("some-program"),
        storage::user_config_directory("some-program")}) {
        INFO(directory.string());

        REQUIRE(directory.is_absolute());
        REQUIRE(directory.filename() == "some-program");
    }
}

TEST_CASE("a program's data and configuration are directories apart", "[jfc::storage::user_directories]") {
    const auto data = storage::user_data_directory("some-program");
    const auto config = storage::user_config_directory("some-program");

    INFO(data.string() << " and " << config.string());

    const auto inside = [](const std::filesystem::path &aOuter, const std::filesystem::path &aInner) {
        return std::mismatch(aOuter.begin(), aOuter.end(), aInner.begin(), aInner.end()).first == aOuter.end();
    };

    REQUIRE_FALSE(inside(data, config));
    REQUIRE_FALSE(inside(config, data));
}

TEST_CASE("a user directory may be named in UTF-8", "[jfc::storage::user_directories]") {
    const std::string name = "Caf\xC3\xA9";

    REQUIRE(storage::user_data_directory(name).filename() == utf8_path(name));
}

TEST_CASE("a program's name is one name", "[jfc::storage::user_directories]") {
    for (const std::string &each : std::vector<std::string>{"", ".", "..", "a/b", "../escaped", "a\\b", "c:", "con",
        "a*b", "trailing.", std::string("a\0b", 3)}) {
        INFO("name \"" << each << "\"");

        REQUIRE_THROWS_AS(storage::user_data_directory(each), storage::exception);
        REQUIRE_THROWS_AS(storage::user_config_directory(each), storage::exception);
    }
}
