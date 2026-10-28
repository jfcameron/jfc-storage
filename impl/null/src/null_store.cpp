// © Joseph Cameron - MIT License

#include <jfc/storage/null_store.h>
#include <jfc/storage/store.h>

using namespace jfc;

namespace {
    class _null_store final : public storage::store {
        [[nodiscard]] std::optional<storage::file_data_type> _load_file(const std::string &) const override {
            return std::nullopt;
        }

        [[nodiscard]] std::optional<storage::file_info> _file_info(const std::string &) const override {
            return std::nullopt;
        }

        void _save_file(const std::string &, storage::file_view_type) override {}

        void _make_directory(const std::string &) override {}

        void _remove_file(const std::string &) override {}

        void _remove_directory(const std::string &) override {}

        void _move_file(const std::string &, const std::string &) override {}

        void _move_directory(const std::string &, const std::string &) override {}

        [[nodiscard]] std::vector<std::string> _files(const std::string &) const override { return {}; }

        [[nodiscard]] std::vector<std::string> _directories(const std::string &) const override { return {}; }
    };
}

storage::store_ptr_type storage::make_null_store() {
    return std::make_unique<_null_store>();
}
