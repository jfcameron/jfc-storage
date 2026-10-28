// © Joseph Cameron - MIT License

#include <jfc/storage/exception.h>

using namespace jfc;

storage::exception::exception(const std::string &aWhat)
: std::runtime_error(aWhat)
{}

storage::exception::exception(const char *const aWhat)
: std::runtime_error(aWhat)
{}

storage::path_refused::path_refused(const std::string &aWhat)
: exception(aWhat)
{}

storage::not_found::not_found(const std::string &aWhat)
: exception(aWhat)
{}
