// © Joseph Cameron - MIT License

#ifndef JFC_STORAGE_PERSISTENT_DIRECTORY_H
#define JFC_STORAGE_PERSISTENT_DIRECTORY_H

#include <filesystem>
#include <functional>

namespace jfc::storage {
    /// \brief persistent storage on browser builds
    ///
    /// Emscripten's filesystem is in memory and lost with the page. IDBFS keeps a directory of it
    /// in IndexedDB.
    /// The user directories are under `/persistent`, so a page mounts that once and synchronises after it writes.
    /// The loading and the saving are asynchronous, so a page that reads before the first load is through finds 
    /// nothing there.
    class persistent_directory final {
    public:
        /// \brief mounts aPath on IndexedDB and starts loading what was stored there
        static void mount(const std::filesystem::path &aPath, std::function<void(bool aSucceeded)> aOnLoaded);

        /// \brief whether the first load of aPath is successful
        [[nodiscard]] static bool is_loaded(const std::filesystem::path &aPath);

        /// \brief starts saving what has changed under aPath to IndexedDB, and tells aOnSaved whether it was saved
        static void synchronise(const std::filesystem::path &aPath,
            std::function<void(bool aSucceeded)> aOnSaved = {});

        /// \brief whether a save of aPath is under way, or waiting to start
        [[nodiscard]] static bool is_synchronising(const std::filesystem::path &aPath);
    };
}

#endif
