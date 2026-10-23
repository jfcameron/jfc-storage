# jfc-storage
Library for reading/writing application data and configuration in platform appropriate locations (~/.local and ~/.config on linux, C:\Users\<Username>\AppData on windows).

## Getting Started
Install cmake 3.21 or greater, and a c++20 compatible compiler 
```
# initialize the tree
git submodule update --init --recursive

# generate the script to build the library on linux using gcc
cmake --preset linux-gcc

# build the library
cmake --build --preset linux-gcc

# run the test suite
ctest --preset linux-gcc
```
See `CMakePresets.json` for the full list of supported platforms and compilers. See `.github/workflows/ci.yml` to see exactly how CI runners build it on different platforms.

## Supported platforms
| system | stores | user directories |
| --- | --- | --- |
| Linux | yes | XDG base directories |
| macOS 10.15+ | yes | `~/Library/Application Support`, `~/Library/Preferences` |
| iOS 13+ | yes | the app container's `Library/Application Support` |
| Windows | yes | the roaming application data known folder |
| Android | yes | none: see below |
| Emscripten | yes | none |

## Paths
Paths are UTF-8 names separated by `/`, relative to the store's root. 
Absolute paths are refused. Paths that climb above the root, `\ : * ? " < > |`, control characters, names ending in `.` or a space, and names Windows keeps for devices (`con`, `nul`, `com1`...) are all refused.
Names beginning `.jfc-storage` are kept for stores' own use. 
Whether `A` and `a` are the same file is up to the system.
