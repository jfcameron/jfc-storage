# © Joseph Cameron - All Rights Reserved

cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(jfcstorage_consumer LANGUAGES CXX)

find_package(jfcstorage REQUIRED)

add_executable(consumer main.cpp)

target_link_libraries(consumer PRIVATE jfc::storage)

set_target_properties(consumer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<0:>")

