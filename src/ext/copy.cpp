// © Joseph Cameron - All Rights Reserved

#include <jfc/storage/exception.h>
#include <jfc/storage/ext/copy.h>

#include <string>
#include <vector>

using namespace jfc;

namespace {
    [[nodiscard]] std::string _in(const std::string_view aDirectory, const std::string &aName) {
        return aDirectory.empty() ? aName : std::string(aDirectory) + "/" + aName;
    }

    [[nodiscard]] storage::exception _failed(const std::string_view aOperation, const std::string_view aPath,
        const std::string_view aWhy) {
        return storage::exception("jfc::storage: cannot " + std::string(aOperation) + " \"" + std::string(aPath)
            + "\": " + std::string(aWhy));
    }

    void _files_below(const storage::store &aStore, const std::string &aDirectory, const std::string &aRelative,
        std::vector<std::string> &aOut) {
        for (const auto &each : aStore.files(aDirectory)) aOut.push_back(_in(aRelative, each));

        for (const auto &each : aStore.directories(aDirectory))
            _files_below(aStore, _in(aDirectory, each), _in(aRelative, each), aOut);
    }
}

void storage::ext::copy_file(const store &aFrom, const std::string_view aFromPath, store &aTo,
    const std::string_view aToPath) {
    const auto contents = aFrom.load_file(aFromPath);

    if (!contents) throw _failed("copy", aFromPath, "there is no file there");

    aTo.save_file(aToPath, *contents);
}

void storage::ext::copy_directory(const store &aFrom, const std::string_view aFromDirectory, store &aTo,
    const std::string_view aToDirectory) {
    if (!aFrom.contains_directory(aFromDirectory))
        throw _failed("copy the directory", aFromDirectory, "there is no directory there");

    if (aTo.contains_directory(aToDirectory)) {
        if (!aTo.files(aToDirectory).empty() || !aTo.directories(aToDirectory).empty())
            throw _failed("copy the directory", aFromDirectory,
                "something is in \"" + std::string(aToDirectory) + "\"");
    }
    else if (aTo.contains_file(aToDirectory))
        throw _failed("copy the directory", aFromDirectory, "a file is at \"" + std::string(aToDirectory) + "\"");

    std::vector<std::string> files;

    _files_below(aFrom, std::string(aFromDirectory), {}, files);

    for (const auto &each : files)
        copy_file(aFrom, _in(aFromDirectory, each), aTo, _in(aToDirectory, each));
}
