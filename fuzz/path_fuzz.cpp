// © Joseph Cameron - MIT License

#include <jfc/storage/exception.h>
#include <jfc/storage/memory_store.h>
#include <jfc/storage/path.h>
#include <jfc/storage/store.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

using namespace jfc;

// Any bytes as a path: path_refusal and the store agree on what is refused, and what is accepted is a place a file
// can be saved, found, loaded and removed again
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *aData, const std::size_t aSize) {
    const std::string_view path(reinterpret_cast<const char *>(aData), aSize);

    const auto refusal = storage::path_refusal(path);

    auto store = storage::make_memory_store();

    try {
        static_cast<void>(store->files(path));

        if (refusal) std::abort();
    }
    catch (const storage::path_refused &) {
        if (!refusal) std::abort();
    }

    if (refusal) return 0;

    const std::byte data[] = {std::byte{42}};

    try {
        store->save_file(path, data);
    }
    catch (const storage::path_refused &) {
        // only for the root, the one directory an empty store has
        if (!store->contains_directory(path)) std::abort();

        return 0;
    }

    if (!store->contains_file(path)) std::abort();

    const auto loaded = store->load_file(path);

    if (!loaded || loaded->size() != 1 || (*loaded)[0] != data[0]) std::abort();

    if (store->files("").empty() && store->directories("").empty()) std::abort();

    store->remove_file(path);

    if (store->contains_file(path)) std::abort();

    return 0;
}
