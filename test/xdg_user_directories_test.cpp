// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>

#include <jfc/storage/user_directories.h>

#include <cstdlib>
#include <optional>
#include <string>
#include <utility>

using namespace jfc;

namespace {
    class environment_variable final {
        std::string mName;
        std::optional<std::string> mPrevious;

    public:
        environment_variable(std::string aName, const char *const aValue)
        : mName(std::move(aName)) {
            if (const char *const previous = ::getenv(mName.c_str())) mPrevious = previous;

            if (aValue) ::setenv(mName.c_str(), aValue, 1);
            else ::unsetenv(mName.c_str());
        }

        environment_variable(const environment_variable &) = delete;
        environment_variable &operator=(const environment_variable &) = delete;

        ~environment_variable() {
            if (mPrevious) ::setenv(mName.c_str(), mPrevious->c_str(), 1);
            else ::unsetenv(mName.c_str());
        }
    };
}

TEST_CASE("the XDG variables say where the user's directories are", "[jfc::storage::user_directories]") {
    const environment_variable data("XDG_DATA_HOME", "/somewhere/data");
    const environment_variable config("XDG_CONFIG_HOME", "/somewhere/config");

    REQUIRE(storage::user_data_directory("program") == "/somewhere/data/program");
    REQUIRE(storage::user_config_directory("program") == "/somewhere/config/program");
}

TEST_CASE("without the XDG variables the user's directories are in their home", "[jfc::storage::user_directories]") {
    const environment_variable home("HOME", "/home/someone");

    SECTION("unset") {
        const environment_variable data("XDG_DATA_HOME", nullptr);
        const environment_variable config("XDG_CONFIG_HOME", nullptr);

        REQUIRE(storage::user_data_directory("program") == "/home/someone/.local/share/program");
        REQUIRE(storage::user_config_directory("program") == "/home/someone/.config/program");
    }

    SECTION("empty") {
        const environment_variable data("XDG_DATA_HOME", "");
        const environment_variable config("XDG_CONFIG_HOME", "");

        REQUIRE(storage::user_data_directory("program") == "/home/someone/.local/share/program");
        REQUIRE(storage::user_config_directory("program") == "/home/someone/.config/program");
    }

    SECTION("relative, which the specification says to ignore") {
        const environment_variable data("XDG_DATA_HOME", "relative/data");
        const environment_variable config("XDG_CONFIG_HOME", "relative/config");

        REQUIRE(storage::user_data_directory("program") == "/home/someone/.local/share/program");
        REQUIRE(storage::user_config_directory("program") == "/home/someone/.config/program");
    }
}

TEST_CASE("without HOME the home is the one on record for the user", "[jfc::storage::user_directories]") {
    const environment_variable home("HOME", nullptr);
    const environment_variable data("XDG_DATA_HOME", nullptr);

    const auto directory = storage::user_data_directory("program");

    REQUIRE(directory.is_absolute());
    REQUIRE(directory.parent_path().filename() == "share");
}
