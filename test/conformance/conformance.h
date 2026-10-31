// © Joseph Cameron - MIT License

#ifndef JFC_STORAGE_CONFORMANCE_H
#define JFC_STORAGE_CONFORMANCE_H

#include <jfc/storage/store.h>

#include <string_view>

namespace jfc::storage::conformance {
    //! a new, empty store. The suite may hold several at once
    [[nodiscard]] store_ptr_type make_store();

    [[nodiscard]] std::string_view implementation();
}

#endif
