// © Joseph Cameron - MIT License

#include <jfc/catch.hpp>

#include <jfc/storage/user_directories.h>

#include <cstdlib>
#include <filesystem>
#include <optional>

using namespace jfc;

namespace {
    [[nodiscard]] std::optional<std::filesystem::path> environment(const wchar_t *const apName) {
        wchar_t *value = nullptr;
        std::size_t length = 0;

        if (_wdupenv_s(&value, &length, apName) != 0 || !value) return std::nullopt;

        std::filesystem::path out(value);

        std::free(value);

        return out;
    }
}

TEST_CASE("on windows data roams and configuration is local", "[jfc::storage::user_directories]") {
    const auto roaming = environment(L"APPDATA");
    const auto local = environment(L"LOCALAPPDATA");

    if (!roaming || !local) {
        WARN("APPDATA or LOCALAPPDATA is unset: nothing to check them against");

        return;
    }

    REQUIRE(storage::user_data_directory("some-program") == *roaming / "some-program");
    REQUIRE(storage::user_config_directory("some-program") == *local / "some-program");
}
