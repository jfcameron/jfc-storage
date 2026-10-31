# jfc-storage
A library for keeping a program's files: saves, settings, downloaded content. It defines a store interface, and provides a store in memory and one that keeps nothing. The stores that leave the process are repositories of their own: [jfc-storage_filesystem](https://github.com/jfcameron/jfc-storage_filesystem) on a disk, and [jfc-storage_idbfs](https://github.com/jfcameron/jfc-storage_idbfs) in a browser's IndexedDB.

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

To use it from another project, add it with `add_subdirectory` and link `jfcstorage` for all of it, or the parts alone: `jfc::storage_interface`, `jfc::storage_memory`, `jfc::storage_null` and `jfc::storage_ext`. It can also be installed, and found with `find_package(jfcstorage)`.

## Overview
### Stores
A store is a tree of files and directories under a root. It loads, saves, moves and removes files, makes, moves and removes directories, and lists what is in a directory, in byte order. Loading a file that is not there gives nothing rather than an error, as does asking for its size and when it was written, and removing something that is not there does nothing. Saving a file makes the directories it is in, and a directory stays when what was in it is removed. A store is held by `std::unique_ptr` to the interface, so a program can be given a store on a disk, and its tests a memory store, without either knowing which it has.

Every failure is thrown as a `jfc::storage::exception`. A path the store refuses is a `path_refused`, and moving or copying something that is not there is a `not_found`, so a program can tell those apart from a disk that is full or a file it may not write.

### Paths
Paths are UTF-8 names separated by `/`, relative to the store's root, and mean the same in every store on every platform. A path that one platform cannot keep is refused on all of them, so a save that works on Linux does not fail on Windows. Absolute paths are refused, as are paths that climb above the root, `\ : * ? " < > |`, control characters, names ending in `.` or a space, and names Windows keeps for devices (`con`, `nul`, `com1`...). Names beginning `.jfc-storage` are kept for stores' own use. `path_refusal` says whether, and why, a path would be refused, to check a name a player typed before saving under it. Whether `A` and `a` are the same file is up to the system.

### Conformance
What the interface promises, every store that keeps files is held to by the same tests. `jfcstorage_conformance` is compiled into each implementation's tests, which supply `make_store`. The null store promises to keep nothing, so its own tests check that instead.

### Implementations
#### `make_memory_store`
A store that keeps its files in memory for as long as it lives. It behaves as every store does, as the conformance suite checks, so it is the store for tests.

#### `make_null_store`
A store that keeps nothing. Every save is discarded and every load and listing is empty. It is for a program run without anywhere to save, or a player who has turned saving off.

### Copying
`ext::copy_file` and `ext::copy_directory` copy between any two stores, through nothing but the interface, so a world can be copied from one kind of store to another, or from one directory to another. A directory is copied as it was when the copy began, empty directories included, so a directory can be copied into itself.

### Threads
A memory store is not safe to use from more than one thread at a time. A null store can be used from any number of threads.
