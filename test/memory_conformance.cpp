// © Joseph Cameron - MIT License

#include "conformance.h"

#include <jfc/storage/memory_store.h>

namespace jfc::storage::conformance {
    store_ptr_type make_store() { return make_memory_store(); }

    std::string_view implementation() { return "memory"; }
}
