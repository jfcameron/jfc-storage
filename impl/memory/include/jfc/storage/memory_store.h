// © Joseph Cameron - All Rights Reserved

#ifndef JFC_STORAGE_MEMORY_STORE_H
#define JFC_STORAGE_MEMORY_STORE_H

#include <jfc/storage/types.h>

namespace jfc::storage {
    /// \brief a store that keeps its files in memory for as long as it lives
    [[nodiscard]] store_ptr_type make_memory_store();
}

#endif
