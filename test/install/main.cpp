// © Joseph Cameron - MIT License

#include <jfc/storage.h>
#include <jfc/storage/ext/copy.h>
#include <jfc/storage/memory_store.h>
#include <jfc/storage/null_store.h>

#include <cstddef>

int main() {
    const std::byte data[] = {std::byte{1}, std::byte{2}};

    if (jfc::storage::make_null_store()->load_file("a/b")) return 1;

    auto memory = jfc::storage::make_memory_store();

    memory->save_file("a/b", data);

    jfc::storage::ext::copy_directory(*memory, "a", *memory, "c");

    if (!memory->contains_file("c/b")) return 1;

    if (!jfc::storage::path_refusal("../x")) return 1;

    return 0;
}
