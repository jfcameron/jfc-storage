// © Joseph Cameron - All Rights Reserved

#include <jfc/storage.h>
#include <jfc/storage/ext/copy.h>
#include <jfc/storage/filesystem_store.h>
#include <jfc/storage/memory_store.h>
#include <jfc/storage/null_store.h>

#if __has_include(<jfc/storage/impl_user_directories.h>) || __has_include(<jfc/storage/impl_flush.h>)
#error "a private header was installed"
#endif

#if __has_include(<jfc/storage/user_directories.h>)
#include <jfc/storage/user_directories.h>
#endif

#include <cstddef>
#include <filesystem>

int main() {
    const auto root = std::filesystem::temp_directory_path() / "jfc-storage-consumer";

    std::filesystem::remove_all(root);

    auto store = jfc::storage::make_filesystem_store(root);

    const std::byte data[] = {std::byte{1}, std::byte{2}};

    store->save_file("a/b", data);

    const auto loaded = store->load_file("a/b");

    std::filesystem::remove_all(root);

    if (!loaded || loaded->size() != 2) return 1;

    if (jfc::storage::make_null_store()->load_file("a/b")) return 1;

    auto memory = jfc::storage::make_memory_store();

    memory->save_file("a/b", data);

    jfc::storage::ext::copy_directory(*memory, "a", *memory, "c");

    if (!memory->contains_file("c/b")) return 1;

    if (!jfc::storage::path_refusal("../x")) return 1;

#if __has_include(<jfc/storage/user_directories.h>)
    if (jfc::storage::user_data_directory("consumer").filename() != "consumer") return 1;
#endif

    return 0;
}
