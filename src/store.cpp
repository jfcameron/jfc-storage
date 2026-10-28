// © Joseph Cameron - MIT License

#include <jfc/storage/exception.h>
#include <jfc/storage/path.h>
#include <jfc/storage/store.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <utility>

using namespace jfc;

namespace {
    [[nodiscard]] std::string _printable(const std::string_view aPath) {
        constexpr std::string_view HEX = "0123456789abcdef";

        std::string out;

        for (const char each : aPath) {
            const auto byte = static_cast<unsigned char>(each);

            if (byte < 0x20 || byte == 0x7F) out += std::string("\\x") + HEX[byte >> 4] + HEX[byte & 0xF];
            else out += each;
        }

        return out;
    }

    [[nodiscard]] std::string _message(const std::string_view aOperation, const std::string_view aPath,
        const std::string_view aWhy) {
        return "jfc::storage: cannot " + std::string(aOperation) + " \"" + _printable(aPath) + "\": "
            + std::string(aWhy);
    }

    [[nodiscard]] storage::path_refused _refused(const std::string_view aOperation, const std::string_view aPath,
        const std::string_view aWhy) {
        return storage::path_refused(_message(aOperation, aPath, aWhy));
    }

    [[nodiscard]] storage::exception _failed(const std::string_view aOperation, const std::string_view aPath,
        const std::string_view aWhy) {
        return storage::exception(_message(aOperation, aPath, aWhy));
    }

    [[nodiscard]] storage::not_found _not_found(const std::string_view aOperation, const std::string_view aPath,
        const std::string_view aWhy) {
        return storage::not_found(_message(aOperation, aPath, aWhy));
    }

    [[nodiscard]] bool _is_utf8(const std::string_view aText) {
        for (std::size_t i = 0; i < aText.size();) {
            const auto lead = static_cast<unsigned char>(aText[i]);

            std::size_t length;
            char32_t codePoint;

            if (lead < 0x80) { ++i; continue; }
            else if ((lead & 0xE0) == 0xC0) { length = 2; codePoint = lead & 0x1F; }
            else if ((lead & 0xF0) == 0xE0) { length = 3; codePoint = lead & 0x0F; }
            else if ((lead & 0xF8) == 0xF0) { length = 4; codePoint = lead & 0x07; }
            else return false;

            if (i + length > aText.size()) return false;

            for (std::size_t j = 1; j < length; ++j) {
                const auto continuation = static_cast<unsigned char>(aText[i + j]);

                if ((continuation & 0xC0) != 0x80) return false;

                codePoint = (codePoint << 6) | (continuation & 0x3F);
            }

            constexpr std::array<char32_t, 5> SMALLEST_FOR_LENGTH{0, 0, 0x80, 0x800, 0x10000};

            if (codePoint < SMALLEST_FOR_LENGTH[length]) return false;
            if (codePoint > 0x10FFFF) return false;
            if (codePoint >= 0xD800 && codePoint <= 0xDFFF) return false;

            i += length;
        }

        return true;
    }

    [[nodiscard]] bool _is_device_name(const std::string_view aName) {
        std::string stem(aName.substr(0, aName.find('.')));

        std::transform(stem.begin(), stem.end(), stem.begin(), [](const char aCharacter) {
            return aCharacter >= 'A' && aCharacter <= 'Z' ? static_cast<char>(aCharacter - 'A' + 'a') : aCharacter;
        });

        if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul" || stem == "conin$" || stem == "conout$")
            return true;

        if (stem.size() < 4 || !(stem.starts_with("com") || stem.starts_with("lpt"))) return false;

        const auto number = std::string_view(stem).substr(3);

        return (number.size() == 1 && number[0] >= '1' && number[0] <= '9')
            || number == "\xC2\xB9" || number == "\xC2\xB2" || number == "\xC2\xB3";
    }

    [[nodiscard]] bool _is_reserved_name(const std::string_view aName) {
        constexpr std::string_view RESERVED = ".jfc-storage";

        if (aName.size() < RESERVED.size()) return false;

        return std::equal(RESERVED.begin(), RESERVED.end(), aName.begin(), [](const char aReserved, const char aEach) {
            return aReserved == std::tolower(static_cast<unsigned char>(aEach));
        });
    }

    [[nodiscard]] std::string_view _unportable_character(const char aCharacter) {
        switch (aCharacter) {
            case '\\': return "it contains '\\'; the separator is '/'";
            case ':': return "it contains ':', which windows reads as a drive or a stream";
            case '*': case '?': case '"': case '<': case '>': case '|':
                return "it contains a character windows does not allow in a name: * ? \" < > |";
            default: break;
        }

        if (const auto byte = static_cast<unsigned char>(aCharacter); byte < 0x20 || byte == 0x7F)
            return "it contains a control character";

        return {};
    }

    struct _resolution final {
        std::string path; 
        std::string_view refusal; 
    };

    [[nodiscard]] _resolution _resolve(const std::string_view aPath) {
        if (!_is_utf8(aPath)) return {{}, "it is not UTF-8"};

        if (aPath.starts_with('/')) return {{}, "it is absolute; paths are relative to the store"};

        for (const char each : aPath)
            if (const auto why = _unportable_character(each); !why.empty()) return {{}, why};

        std::vector<std::string_view> names;

        for (std::size_t begin = 0; begin <= aPath.size();) {
            const auto end = std::min(aPath.find('/', begin), aPath.size());
            const auto name = aPath.substr(begin, end - begin);

            begin = end + 1;

            if (name.empty() || name == ".") continue;

            if (name == "..") {
                if (names.empty()) return {{}, "it climbs above the store's root"};

                names.pop_back();

                continue;
            }

            if (name.ends_with('.') || name.ends_with(' '))
                return {{}, "a name ends in '.' or ' ', which windows removes"};

            if (_is_device_name(name)) return {{}, "a name is one windows reserves for a device"};

            if (_is_reserved_name(name))
                return {{}, "a name begins \".jfc-storage\", which stores keep for their own use"};

            names.push_back(name);
        }

        _resolution out;

        for (const auto name : names) {
            if (!out.path.empty()) out.path += '/';

            out.path += name;
        }

        return out;
    }

    [[nodiscard]] std::string _resolved(const std::string_view aOperation, const std::string_view aPath) {
        auto resolution = _resolve(aPath);

        if (!resolution.refusal.empty()) throw _refused(aOperation, aPath, resolution.refusal);

        return std::move(resolution.path);
    }

    [[nodiscard]] std::string _resolved_file(const std::string_view aOperation, const std::string_view aPath) {
        auto out = _resolved(aOperation, aPath);

        if (out.empty()) throw _refused(aOperation, aPath, "it names the store's root, not a file");

        return out;
    }

    [[nodiscard]] std::string _resolved_directory(const std::string_view aOperation, const std::string_view aPath) {
        auto out = _resolved(aOperation, aPath);

        if (out.empty()) throw _refused(aOperation, aPath, "it is the store's root");

        return out;
    }
}

std::optional<std::string> storage::path_refusal(const std::string_view aPath) {
    const auto resolution = _resolve(aPath);

    if (resolution.refusal.empty()) return std::nullopt;

    return std::string(resolution.refusal);
}

std::optional<storage::file_data_type> storage::store::load_file(const std::string_view aPath) const {
    return _load_file(_resolved_file("load", aPath));
}

bool storage::store::contains_file(const std::string_view aPath) const {
    return _contains_file(_resolved_file("look for the file", aPath));
}

std::optional<storage::file_info> storage::store::file_info(const std::string_view aPath) const {
    return _file_info(_resolved_file("look at", aPath));
}

bool storage::store::contains_directory(const std::string_view aDirectory) const {
    const auto directory = _resolved("look for the directory", aDirectory);

    return directory.empty() || _contains_directory(directory);
}

std::vector<std::string> storage::store::files(const std::string_view aDirectory) const {
    return _files(_resolved("list the files in", aDirectory));
}

std::vector<std::string> storage::store::directories(const std::string_view aDirectory) const {
    return _directories(_resolved("list the directories in", aDirectory));
}

void storage::store::save_file(const std::string_view aPath, const file_view_type aData) {
    _save_file(_resolved_file("save", aPath), aData);
}

void storage::store::make_directory(const std::string_view aDirectory) {
    const auto directory = _resolved("make the directory", aDirectory);

    if (directory.empty() || _contains_directory(directory)) return;

    if (_contains_file(directory)) throw _failed("make the directory", aDirectory, "a file is there");

    _make_directory(directory);
}

void storage::store::remove_file(const std::string_view aPath) {
    const auto path = _resolved_file("remove", aPath);

    if (_contains_file(path)) _remove_file(path);
}

void storage::store::remove_directory(const std::string_view aDirectory) {
    const auto directory = _resolved_directory("remove the directory", aDirectory);

    if (_contains_directory(directory)) _remove_directory(directory);
}

void storage::store::move_file(const std::string_view aOldPath, const std::string_view aNewPath) {
    const auto oldPath = _resolved_file("move from", aOldPath);
    const auto newPath = _resolved_file("move to", aNewPath);

    if (!_contains_file(oldPath)) throw _not_found("move", aOldPath, "there is no file there");

    if (_contains_directory(newPath))
        throw _failed("move", aOldPath, "a directory is at \"" + newPath + "\"");

    _move_file(oldPath, newPath);
}

void storage::store::move_directory(const std::string_view aOldDirectory, const std::string_view aNewDirectory) {
    const auto oldDirectory = _resolved_directory("move the directory from", aOldDirectory);
    const auto newDirectory = _resolved_directory("move the directory to", aNewDirectory);

    if (newDirectory == oldDirectory || newDirectory.starts_with(oldDirectory + "/"))
        throw _refused("move the directory to", aNewDirectory, "it is in the directory being moved");

    if (!_contains_directory(oldDirectory))
        throw _not_found("move the directory", aOldDirectory, "there is no directory there");

    if (_contains_file(newDirectory) || _contains_directory(newDirectory))
        throw _failed("move the directory", aOldDirectory, "something is at \"" + newDirectory + "\"");

    _move_directory(oldDirectory, newDirectory);
}

bool storage::store::_contains_file(const std::string &aPath) const {
    return _load_file(aPath).has_value();
}

bool storage::store::_contains_directory(const std::string &aDirectory) const {
    const auto slash = aDirectory.rfind('/');

    const auto parent = slash == std::string::npos ? std::string() : aDirectory.substr(0, slash);
    const auto name = slash == std::string::npos ? aDirectory : aDirectory.substr(slash + 1);

    const auto listed = _directories(parent);

    return std::binary_search(listed.begin(), listed.end(), name);
}
