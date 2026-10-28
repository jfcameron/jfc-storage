# © Joseph Cameron - MIT License

cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(jfcstorage
    VERSION 1.0.0
    DESCRIPTION "crossplatform program storage manager"
    LANGUAGES CXX)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/jfc-cmake/jfclib.cmake")

include(GNUInstallDirs)

option(JFCSTORAGE_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(JFCSTORAGE_BUILD_DOCS "Build documentation" OFF)

if (ANDROID)
    set(_jfcstorage_user_directories "")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME MATCHES "BSD$")
    set(_jfcstorage_user_directories "xdg")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(_jfcstorage_user_directories "macos")
elseif (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(_jfcstorage_user_directories "ios")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(_jfcstorage_user_directories "windows")
elseif (EMSCRIPTEN)
    set(_jfcstorage_user_directories "emscripten")
else()
    set(_jfcstorage_user_directories "")
endif()

set(_jfcstorage_strict_flags)

if (MSVC)
    list(APPEND _jfcstorage_strict_flags $<$<COMPILE_LANGUAGE:CXX>:/permissive->)
else()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        list(APPEND _jfcstorage_strict_flags $<$<COMPILE_LANGUAGE:CXX>:-pedantic>)
    endif()

    include(CheckCXXCompilerFlag)

    foreach (_strict_flag "-Werror=c++20-extensions" "-Werror=c++23-extensions")
        string(MAKE_C_IDENTIFIER "HAS${_strict_flag}" _strict_flag_symbol)

        check_cxx_compiler_flag("${_strict_flag}" ${_strict_flag_symbol})

        if (${_strict_flag_symbol})
            list(APPEND _jfcstorage_strict_flags $<$<COMPILE_LANGUAGE:CXX>:${_strict_flag}>)
        endif()
    endforeach()
endif()

function(_jfcstorage_library aTarget aExportName)
    add_library(${aTarget} STATIC ${ARGN})

    add_library(jfc::${aExportName} ALIAS ${aTarget})

    set_target_properties(${aTarget} PROPERTIES
        EXPORT_NAME ${aExportName}
        PREFIX "lib"
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

    target_compile_features(${aTarget} PUBLIC cxx_std_20)

    target_compile_options(${aTarget} PRIVATE ${_jfcstorage_strict_flags})
endfunction()

_jfcstorage_library(jfcstorage_interface storage_interface
    "${CMAKE_CURRENT_SOURCE_DIR}/src/exception.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/store.cpp")

target_include_directories(jfcstorage_interface PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

_jfcstorage_library(jfcstorage_null storage_null
    "${CMAKE_CURRENT_SOURCE_DIR}/impl/null/src/null_store.cpp")

target_include_directories(jfcstorage_null PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/impl/null/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

target_link_libraries(jfcstorage_null PUBLIC jfcstorage_interface)

_jfcstorage_library(jfcstorage_memory storage_memory
    "${CMAKE_CURRENT_SOURCE_DIR}/impl/memory/src/memory_store.cpp")

target_include_directories(jfcstorage_memory PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/impl/memory/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

target_link_libraries(jfcstorage_memory PUBLIC jfcstorage_interface)

if (WIN32)
    set(_jfcstorage_flush "windows")
else()
    set(_jfcstorage_flush "posix")
endif()

_jfcstorage_library(jfcstorage_filesystem storage_filesystem
    "${CMAKE_CURRENT_SOURCE_DIR}/impl/filesystem/src/filesystem_store.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/impl/filesystem/src/${_jfcstorage_flush}_flush.cpp")

target_include_directories(jfcstorage_filesystem
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/impl/filesystem/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/impl/filesystem/src/include")

target_link_libraries(jfcstorage_filesystem PUBLIC jfcstorage_interface)

_jfcstorage_library(jfcstorage_ext storage_ext
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ext/copy.cpp")

target_link_libraries(jfcstorage_ext PUBLIC jfcstorage_interface)

set(_jfcstorage_targets
    jfcstorage_interface jfcstorage_ext jfcstorage_null jfcstorage_memory jfcstorage_filesystem)

if (_jfcstorage_user_directories)
    _jfcstorage_library(jfcstorage_user_directories storage_user_directories
        "${CMAKE_CURRENT_SOURCE_DIR}/impl/user_directories/src/user_directories.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/impl/user_directories/src/${_jfcstorage_user_directories}_user_directories.cpp")

    target_include_directories(jfcstorage_user_directories
        PUBLIC
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/impl/user_directories/include>"
            "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/impl/user_directories/src/include")

    target_link_libraries(jfcstorage_user_directories PUBLIC jfcstorage_interface)

    if (_jfcstorage_user_directories STREQUAL "windows")
        target_link_libraries(jfcstorage_user_directories PRIVATE shell32 ole32)
    endif()

    list(APPEND _jfcstorage_targets jfcstorage_user_directories)
endif()

if (EMSCRIPTEN)
    _jfcstorage_library(jfcstorage_persistent storage_persistent
        "${CMAKE_CURRENT_SOURCE_DIR}/impl/emscripten/src/persistent_directory.cpp")

    target_include_directories(jfcstorage_persistent PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/impl/emscripten/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

    target_link_libraries(jfcstorage_persistent PUBLIC jfcstorage_interface PRIVATE jfcstorage_filesystem)

    target_link_options(jfcstorage_persistent PUBLIC "-lidbfs.js")

    list(APPEND _jfcstorage_targets jfcstorage_persistent)
endif()

add_library(jfcstorage INTERFACE)

add_library(jfc::storage ALIAS jfcstorage)

set_target_properties(jfcstorage PROPERTIES EXPORT_NAME storage)

target_link_libraries(jfcstorage INTERFACE ${_jfcstorage_targets})

list(APPEND _jfcstorage_targets jfcstorage)

include(CMakePackageConfigHelpers)

install(TARGETS ${_jfcstorage_targets}
    EXPORT jfcstorage-targets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")

set(_jfcstorage_public_include_directories
    "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/impl/null/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/impl/memory/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/impl/filesystem/include/")

if (_jfcstorage_user_directories)
    list(APPEND _jfcstorage_public_include_directories
        "${CMAKE_CURRENT_SOURCE_DIR}/impl/user_directories/include/")
endif()

if (EMSCRIPTEN)
    list(APPEND _jfcstorage_public_include_directories
        "${CMAKE_CURRENT_SOURCE_DIR}/impl/emscripten/include/")
endif()

install(DIRECTORY ${_jfcstorage_public_include_directories}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(EXPORT jfcstorage-targets
    FILE jfcstorage-targets.cmake
    NAMESPACE jfc::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/jfcstorage")

write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/jfcstorage-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

file(WRITE "${PROJECT_BINARY_DIR}/jfcstorage-config.cmake"
"include(\"\${CMAKE_CURRENT_LIST_DIR}/jfcstorage-targets.cmake\")
")

install(FILES
    "${PROJECT_BINARY_DIR}/jfcstorage-config.cmake"
    "${PROJECT_BINARY_DIR}/jfcstorage-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/jfcstorage")

if (JFCSTORAGE_BUILD_TESTS)
    add_subdirectory(test)
endif()

if (JFC_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()

if (JFCSTORAGE_BUILD_DOCS)
    add_subdirectory(docs)
endif()
