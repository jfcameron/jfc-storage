// © Joseph Cameron - All Rights Reserved

#ifndef JFC_STORAGE_H
#define JFC_STORAGE_H

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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

    /// @brief read/write files on disk in a directory provided by the OS for this program 
    class store final {
        std::filesystem::path mRoot;

        std::optional<std::filesystem::path> resolve_path(std::string_view aPath) const;
    public:
        /// @brief loads file contents to memory
        /// @param aPath path to the file
        /// @return contents
        std::optional<std::vector<std::byte>> load_file(std::string_view aPath) const;

        /// @brief saves data to file 
        /// @param aPath path to the file
        /// @param aData data
        void save_file(std::string_view aPath, std::span<const std::byte> aData);

        /// @brief removes a file from storage
        /// @param aPath path to the file
        void remove_file(std::string_view aPath);

        /// @brief moves a file in storage from one path to another
        /// @param aOldPath initial path of the file
        /// @param aNewPath new path of the file
        void move_file(std::string_view aOldPath, std::string_view aNewPath);

        store(std::string_view aProgramName);
    };
}

#endif
