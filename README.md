# jfc-storage
A library for keeping a program's files: saves, settings, downloaded content. It defines a store interface and finds the directories each platform expects a program to keep its user's data and configuration in.

API documentation: https://jfcameron.github.io/jfc-storage/

## Getting Started
Install cmake 3.21 or greater, and a c++20 compatible compiler
```
# initialize the tree
git submodule update --init --recursive

# generate the script to build the library and its tests on linux using gcc
cmake --preset linux-gcc

# build the library and tests
cmake --build --preset linux-gcc

# run the tests
ctest --preset linux-gcc
```
The `linux-clang-fuzz` preset builds the fuzzers instead. See `CMakePresets.json` for the full list of supported platforms and compilers. See `.github/workflows/ci.yml` to see exactly how CI runners build it on different platforms.

To use it from another project, add it with `add_subdirectory` and link `jfcstorage` for everything the platform has, or the parts alone: `jfc::storage_interface`, `jfc::storage_filesystem`, `jfc::storage_memory`, `jfc::storage_null`, `jfc::storage_ext`, `jfc::storage_user_directories` and, on Emscripten, `jfc::storage_persistent`. It can also be installed, and found with `find_package(jfcstorage)`.

## Overview
### Stores
A store is a tree of files and directories under a root. It loads, saves, moves and removes files, makes, moves and removes directories, and lists what is in a directory, in byte order. Loading a file that is not there gives nothing rather than an error, as does asking for its size and when it was written, and removing something that is not there does nothing. Saving a file makes the directories it is in, and a directory stays when what was in it is removed. A store is held by `std::unique_ptr` to the interface, so a program can be given a filesystem store, and its tests a memory store, without either knowing which it has.

Every failure is thrown as a `jfc::storage::exception`. A path the store refuses is a `path_refused`, and moving or copying something that is not there is a `not_found`, so a program can tell those apart from a disk that is full or a file it may not write.

### Paths
Paths are UTF-8 names separated by `/`, relative to the store's root, and mean the same in every store on every platform. A path that one platform cannot keep is refused on all of them, so a save that works on Linux does not fail on Windows. Absolute paths are refused, as are paths that climb above the root, `\ : * ? " < > |`, control characters, names ending in `.` or a space, and names Windows keeps for devices (`con`, `nul`, `com1`...). Names beginning `.jfc-storage` are kept for stores' own use. `path_refusal` says whether, and why, a path would be refused, to check a name a player typed before saving under it. Whether `A` and `a` are the same file is up to the system.

### Implementations
#### `make_filesystem_store`
A store whose files are files under a directory. A save writes a temporary file beside the one it replaces and renames it into place, so a reader sees the old file or the new one and never part of either, and two stores saving the same file at once, in the same process or another, leave one of the two whole. With `flush_to_disk`, a change returns only once it is on the disk, so a power cut cannot undo it. With `restrict_to_owner`, the store's directories and the files it saves can only be read by the user who saved them, which suits a login token. This does nothing on Windows, where a user's application data is already their own.

No path leaves the root, but a link already in it is followed wherever it leads. A link to a directory is listed as a directory, unless it leads back to the directory it is in or one above it, which a copy would otherwise follow until the path was too long. A file in the directory that the store would refuse to name, one put there by another program, is not listed.

#### `make_memory_store`
A store that keeps its files in memory for as long as it lives. It behaves as the filesystem store does, as the shared tests check, so it is the store for tests.

#### `make_null_store`
A store that keeps nothing. Every save is discarded and every load and listing is empty. It is for a program run without anywhere to save, or a player who has turned saving off.

### Copying
`ext::copy_file` and `ext::copy_directory` copy between any two stores, through nothing but the interface, so a world can be copied from a memory store to the disk, or from one directory to another. A directory is copied as it was when the copy began, empty directories included, so a directory can be copied into itself.

### User directories
`user_data_directory` and `user_config_directory` give the directories a platform expects a program to keep its user's data and configuration in, which a program makes a filesystem store at. The program's name is checked as a path is, so it cannot climb out of them.

| system | data | configuration |
| --- | --- | --- |
| Linux and BSD | `$XDG_DATA_HOME/<name>`, by default `~/.local/share/<name>` | `$XDG_CONFIG_HOME/<name>`, by default `~/.config/<name>` |
| macOS 10.15+ | `~/Library/Application Support/<name>` | `~/Library/Preferences/<name>` |
| iOS 13+ | the app container's `Library/Application Support/<name>` | the app container's `Library/Preferences/<name>` |
| Windows | `%APPDATA%\<name>`, which roams with the user | `%LOCALAPPDATA%\<name>`, which stays with the machine |
| Emscripten | `/persistent/data/<name>` | `/persistent/config/<name>` |
| Android | none | none |

Android has no user directories: an app's directory is its `Context`'s `getFilesDir()`, which only Java can ask for, so a program makes a filesystem store at the path its Java side passes down.

### Browsers
A page's filesystem is in memory and is lost when the page closes. `persistent_directory` mounts a directory on IndexedDB, loads what was kept there, and saves what has changed when asked to synchronise. Both are asynchronous, and report how they went to a callback. The user directories are under `/persistent`, so a page mounts that once, before anything reads from it.

`make_persistent_store` makes a filesystem store in a mounted directory that synchronises after every change, and tells its callback how each synchronisation went. Changes made while one is under way are saved together by the next. It refuses to change anything before the mount's first load is through, since the load would replace what it had written.

### Threads
A memory store is not safe to use from more than one thread at a time. A filesystem store keeps nothing of its own, so it can be used from any number of threads, as can a null store. Two filesystem stores on the same directory, in one process or two, are as safe together as one is, though when both save the same file it is the last rename that is kept.
