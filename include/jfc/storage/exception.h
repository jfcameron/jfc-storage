// © Joseph Cameron - All Rights Reserved

#ifndef JFC_STORAGE_EXCEPTION_H
#define JFC_STORAGE_EXCEPTION_H

#include <string>
#include <exception>

namespace jfc::storage {
    class exception : public std::exception {
        std::string mWhat = "jfc::storage::exception";
    protected: 
        exception() = default;
    public:
        exception(std::string aWhat) : mWhat(aWhat) {}
        virtual const char *what() const noexcept override { return mWhat.c_str(); }
        virtual ~exception() override = default;
    };
}

#endif
