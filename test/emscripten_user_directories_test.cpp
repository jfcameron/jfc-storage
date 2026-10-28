// © Joseph Cameron - MIT License

#include <jfc/catch.hpp>

#include <jfc/storage/user_directories.h>

using namespace jfc;

TEST_CASE("a page's user directories are under the persistent root", "[jfc::storage::user_directories]") {
    REQUIRE(storage::user_data_directory("program") == "/persistent/data/program");
    REQUIRE(storage::user_config_directory("program") == "/persistent/config/program");
}
