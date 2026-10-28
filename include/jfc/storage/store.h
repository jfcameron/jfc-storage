// © Joseph Cameron - MIT License

#ifndef JFC_STORAGE_STORE_H
#define JFC_STORAGE_STORE_H

#include <jfc/storage/types.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace jfc::storage {
    class store {
    public:
        /// \brief the contents of a file, or nothing if there is no file at aPath
        [[nodiscard]] std::optional<file_data_type> load_file(std::string_view aPath) const;

        /// \brief whether there is a file at aPath
        [[nodiscard]] bool contains_file(std::string_view aPath) const;

        /// \brief the size of the file at aPath and when it was written, or nothing if there is no file there
        [[nodiscard]] std::optional<storage::file_info> file_info(std::string_view aPath) const;

        /// \brief whether there is a directory at aDirectory
        [[nodiscard]] bool contains_directory(std::string_view aDirectory) const;

        /// \brief names of the files directly in a directory, in byte order
        [[nodiscard]] std::vector<std::string> files(std::string_view aDirectory) const;

        /// \brief names of the directories directly in a directory
        [[nodiscard]] std::vector<std::string> directories(std::string_view aDirectory) const;

        /// \brief write a file, replacing any already there
        void save_file(std::string_view aPath, file_view_type aData);

        /// \brief make a directory, and those it is in; nothing if it is already there
        void make_directory(std::string_view aDirectory);

        /// \brief remove a file
        void remove_file(std::string_view aPath);

        /// \brief remove a directory and everything in it
        void remove_directory(std::string_view aDirectory);

        /// \brief move a file, replacing any file already at aNewPath
        void move_file(std::string_view aOldPath, std::string_view aNewPath);

        /// \brief move a directory and everything in it
        void move_directory(std::string_view aOldDirectory, std::string_view aNewDirectory);

        store(const store &) = delete;
        store &operator=(const store &) = delete;

        virtual ~store() = default;

    protected:
        store() = default;

    private:
        [[nodiscard]] virtual std::optional<file_data_type> _load_file(const std::string &aPath) const = 0;

        [[nodiscard]] virtual bool _contains_file(const std::string &aPath) const;

        [[nodiscard]] virtual std::optional<storage::file_info> _file_info(const std::string &aPath) const = 0;

        [[nodiscard]] virtual bool _contains_directory(const std::string &aDirectory) const;

        [[nodiscard]] virtual std::vector<std::string> _files(const std::string &aDirectory) const = 0;

        [[nodiscard]] virtual std::vector<std::string> _directories(const std::string &aDirectory) const = 0;

        virtual void _save_file(const std::string &aPath, file_view_type aData) = 0;

        virtual void _make_directory(const std::string &aDirectory) = 0;

        virtual void _remove_file(const std::string &aPath) = 0;

        virtual void _remove_directory(const std::string &aDirectory) = 0;

        virtual void _move_file(const std::string &aOldPath, const std::string &aNewPath) = 0;

        virtual void _move_directory(const std::string &aOldDirectory, const std::string &aNewDirectory) = 0;
    };
}

#endif
