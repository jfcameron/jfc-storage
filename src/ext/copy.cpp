// © Joseph Cameron - MIT License

#include <jfc/storage/exception.h>
#include <jfc/storage/ext/copy.h>

#include <string>
#include <string_view>
#include <vector>

using namespace jfc;

namespace {
    [[nodiscard]] std::string _in(const std::string_view aDirectory, const std::string &aName) {
        return aDirectory.empty() ? aName : std::string(aDirectory) + "/" + aName;
    }

    [[nodiscard]] std::string _message(const std::string_view aOperation, const std::string_view aPath,
        const std::string_view aWhy) {
        return "jfc::storage: cannot " + std::string(aOperation) + " \"" + std::string(aPath) + "\": " + std::string(aWhy);
    }

    [[nodiscard]] storage::exception _failed(const std::string_view aOperation, const std::string_view aPath,
        const std::string_view aWhy) {
        return storage::exception(_message(aOperation, aPath, aWhy));
    }

    [[nodiscard]] storage::not_found _not_found(const std::string_view aOperation, const std::string_view aPath,
        const std::string_view aWhy) {
        return storage::not_found(_message(aOperation, aPath, aWhy));
    }

    void _below(const storage::store &aStore, std::string_view aDirectory, std::string_view aRelative,
        std::vector<std::string> &aDirectories, std::vector<std::string> &aFiles) {
        for (const auto &each : aStore.files(aDirectory)) aFiles.push_back(_in(aRelative, each));

        for (const auto &each : aStore.directories(aDirectory)) {
            aDirectories.push_back(_in(aRelative, each));

            _below(aStore, _in(aDirectory, each), _in(aRelative, each), aDirectories, aFiles);
        }
    }
}

void storage::ext::copy_file(const store &aFrom, const std::string_view aFromPath, store &aTo,
    const std::string_view aToPath) {
    const auto contents = aFrom.load_file(aFromPath);

    if (!contents) throw _not_found("copy", aFromPath, "there is no file there");

    aTo.save_file(aToPath, *contents);
}

void storage::ext::copy_directory(const store &aFrom, const std::string_view aFromDirectory, store &aTo,
    const std::string_view aToDirectory) {
    if (!aFrom.contains_directory(aFromDirectory))
        throw _not_found("copy the directory", aFromDirectory, "there is no directory there");

    if (aTo.contains_directory(aToDirectory)) {
        if (!aTo.files(aToDirectory).empty() || !aTo.directories(aToDirectory).empty())
            throw _failed("copy the directory", aFromDirectory,
                "something is in \"" + std::string(aToDirectory) + "\"");
    }
    else if (aTo.contains_file(aToDirectory))
        throw _failed("copy the directory", aFromDirectory, "a file is at \"" + std::string(aToDirectory) + "\"");

    std::vector<std::string> directories, files;

    _below(aFrom, std::string(aFromDirectory), {}, directories, files);

    aTo.make_directory(aToDirectory);

    for (const auto &each : directories) aTo.make_directory(_in(aToDirectory, each));

    for (const auto &each : files)
        copy_file(aFrom, _in(aFromDirectory, each), aTo, _in(aToDirectory, each));
}
