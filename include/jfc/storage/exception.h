// © Joseph Cameron - All Rights Reserved

#ifndef JFC_STORAGE_EXCEPTION_H
#define JFC_STORAGE_EXCEPTION_H

#include <stdexcept>
#include <string>

namespace jfc::storage {
    class exception : public std::runtime_error {
    public:
        explicit exception(const std::string &aWhat);
        explicit exception(const char *const aWhat);
    };

    class path_refused final : public exception {
    public:
        explicit path_refused(const std::string &aWhat);
    };
}

#endif
