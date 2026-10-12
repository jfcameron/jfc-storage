// © Joseph Cameron - All Rights Reserved

#ifndef JFC_STORAGE_NULL_STORE_H
#define JFC_STORAGE_NULL_STORE_H

#include <jfc/storage/types.h>

namespace jfc::storage {
    /// \brief a store that holds nothing: every save is discarded, and every load and listing is empty
    [[nodiscard]] store_ptr_type make_null_store();
}

#endif
