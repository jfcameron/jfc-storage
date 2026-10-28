# © Joseph Cameron - MIT License

jfc_add_fuzz_tests(
    C++_STANDARD 20

    FUZZ_SOURCE_FILES
        "${CMAKE_CURRENT_LIST_DIR}/filesystem_fuzz.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/path_fuzz.cpp"

    CORPUS_DIRECTORIES
        "${CMAKE_CURRENT_LIST_DIR}/corpus/paths"

    LIBRARIES
        jfcstorage
)
