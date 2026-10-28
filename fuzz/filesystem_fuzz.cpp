// © Joseph Cameron - MIT License

#include <jfc/storage/exception.h>
#include <jfc/storage/filesystem_store.h>
#include <jfc/storage/memory_store.h>
#include <jfc/storage/path.h>
#include <jfc/storage/store.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <string>
#include <string_view>
#include <vector>

using namespace jfc;

namespace {
    [[nodiscard]] const std::filesystem::path &_root() {
        static const auto root = std::filesystem::temp_directory_path()
            / ("jfcstorage-fuzz-" + std::to_string(std::random_device{}()));

        return root;
    }

    void _tree(const storage::store &aStore, const std::string &aDirectory, std::vector<std::string> &aOut) {
        const auto in = [&](const std::string &aName) { return aDirectory.empty() ? aName : aDirectory + "/" + aName; };

        for (const auto &each : aStore.files(aDirectory)) {
            const auto contents = aStore.load_file(in(each));

            aOut.push_back("file " + in(each) + " " + std::to_string(contents ? contents->size() : 999));
        }

        for (const auto &each : aStore.directories(aDirectory)) {
            aOut.push_back("directory " + in(each));

            _tree(aStore, in(each), aOut);
        }
    }

    [[nodiscard]] std::vector<std::string> _tree(const storage::store &aStore) {
        std::vector<std::string> out;

        _tree(aStore, {}, out);

        return out;
    }

    [[nodiscard]] bool _too_long_for_a_filesystem(const std::string_view aPath) {
        constexpr std::size_t LONGEST_NAME = 255;

        if (_root().native().size() + 1 + aPath.size() >= 4096) return true;

        for (std::size_t begin = 0; begin <= aPath.size();) {
            const auto end = std::min(aPath.find('/', begin), aPath.size());

            if (end - begin > LONGEST_NAME) return true;

            begin = end + 1;
        }

        return false;
    }

    enum class _outcome { done, refused, failed };

    template<typename Operation>
    [[nodiscard]] _outcome _try(const Operation aOperation) {
        try {
            aOperation();

            return _outcome::done;
        }
        catch (const storage::path_refused &) { return _outcome::refused; }
        catch (const storage::exception &) { return _outcome::failed; }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *aData, const std::size_t aSize) {
    const std::string_view path(reinterpret_cast<const char *>(aData), aSize);

    const auto refusal = storage::path_refusal(path);

    auto disk = storage::make_filesystem_store(_root());
    auto memory = storage::make_memory_store();

    if ((_try([&] { static_cast<void>(disk->files(path)); }) == _outcome::refused) != refusal.has_value()) std::abort();

    if (refusal) return 0;

    const auto same = [&](const auto aOperation) {
        const auto onDisk = _try([&] { aOperation(*disk); });
        const auto inMemory = _try([&] { aOperation(*memory); });

        if (onDisk == _outcome::failed && inMemory == _outcome::done && _too_long_for_a_filesystem(path)) return false;

        if (onDisk != inMemory) std::abort();

        if (_tree(*disk) != _tree(*memory)) std::abort();

        return true;
    };

    const std::byte data[] = {std::byte{42}};

    if (same([&](storage::store &aStore) { aStore.save_file(path, data); })
        && same([&](storage::store &aStore) { aStore.make_directory(path); })
        && same([&](storage::store &aStore) { aStore.remove_file(path); })
        && same([&](storage::store &aStore) { aStore.make_directory(path); })) {
        static_cast<void>(same([&](storage::store &aStore) { aStore.save_file(path, data); }));
    }

    std::filesystem::remove_all(_root());

    return 0;
}
