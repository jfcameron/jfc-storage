# jfc-storage
Cross-platform file storage for application data

## Systems

| system | stores | user directories |
| --- | --- | --- |
| Linux | yes | XDG base directories |
| macOS 10.15+ | yes | `~/Library/Application Support`, `~/Library/Preferences` |
| iOS 13+ | yes | the app container's `Library/Application Support` |
| Windows | yes | the roaming application data known folder |
| Android | yes | none: see below |
| Emscripten | yes | none |

## Building

```sh
cmake --preset linux-gcc && cmake --build --preset linux-gcc && ctest --preset linux-gcc
```

`CMakePresets.json` lists every supported build. 

## Paths

Paths are UTF-8 names separated by `/`, relative to the store's root. 
Absolute paths are refused. Paths that climb above the root, `\ : * ? " < > |`, control characters, names ending in `.` or a space, and names Windows keeps for devices (`con`, `nul`, `com1`...) are all refused.
Names beginning `.jfc-storage` are kept for stores' own use. 
Whether `A` and `a` are the same file is up to the system.

`jfc::storage::path_refusal(path)` says why a path would be refused, or nothing, for checking a name before it is
used -- a player's name for a saved world, say. A listing only names what a path can reach, so a file put in a
store's directory from outside under a refused name is not listed.

Paths are checked as written: a filesystem store follows links, so a link placed under its root can lead outside it.

