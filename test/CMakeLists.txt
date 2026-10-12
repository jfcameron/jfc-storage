# © Joseph Cameron - All Rights Reserved

set(_jfcstorage_test_sources
    "${CMAKE_CURRENT_LIST_DIR}/filesystem_store_test.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/null_store_test.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/store_contract_test.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/store_paths_test.cpp")

if (_jfcstorage_user_directories)
    list(APPEND _jfcstorage_test_sources "${CMAKE_CURRENT_LIST_DIR}/user_directories_test.cpp")

    if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/${_jfcstorage_user_directories}_user_directories_test.cpp")
        list(APPEND _jfcstorage_test_sources
            "${CMAKE_CURRENT_LIST_DIR}/${_jfcstorage_user_directories}_user_directories_test.cpp")
    endif()
endif()

jfc_add_tests(
    C++_STANDARD 20

    TEST_SOURCE_FILES
        ${_jfcstorage_test_sources}

    LIBRARIES
        jfcstorage
)

set_property(TARGET ${JFC_TEST_TARGET} PROPERTY CXX_EXTENSIONS OFF)

set(_jfcstorage_header_checks)

foreach (_directory IN LISTS _jfcstorage_public_include_directories)
    file(GLOB_RECURSE _headers RELATIVE "${_directory}" "${_directory}/*.h")

    foreach (_header IN LISTS _headers)
        string(MAKE_C_IDENTIFIER "${_header}" _name)

        set(_check "${CMAKE_CURRENT_BINARY_DIR}/header_checks/${_name}.cpp")

        file(CONFIGURE OUTPUT "${_check}" CONTENT "#include <${_header}>\n")

        list(APPEND _jfcstorage_header_checks "${_check}")
    endforeach()
endforeach()

add_library(jfcstorage_header_checks OBJECT ${_jfcstorage_header_checks})

set_target_properties(jfcstorage_header_checks PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

target_link_libraries(jfcstorage_header_checks PRIVATE jfcstorage)

if (NOT CMAKE_CROSSCOMPILING)
    add_test(NAME jfcstorage_install_and_consume
        COMMAND "${CMAKE_COMMAND}"
            "-DBUILD_DIR=${PROJECT_BINARY_DIR}"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/install_and_consume"
            "-DCONFIG=$<CONFIG>"
            "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
            "-DEXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
            -P "${CMAKE_CURRENT_LIST_DIR}/install/install_and_consume.cmake")
endif()
