# © Joseph Cameron - All Rights Reserved

jfc_add_tests(
    C++_STANDARD 20
    C_STANDARD 90

    TEST_SOURCE_FILES
        "${CMAKE_CURRENT_LIST_DIR}/store_test.cpp"

    LIBRARIES
        "${${PROJECT_NAME}_LIBRARIES}"

    INCLUDE_DIRECTORIES
        "${${PROJECT_NAME}_INCLUDE_DIRECTORIES}"
)
