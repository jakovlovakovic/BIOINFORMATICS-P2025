cmake_minimum_required(VERSION 3.10)
project(BambooFilterProject LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(xxhash STATIC hash/xxhash.c)

add_executable(BambooFilter
    main.cpp
    BambooFilter.cpp
    BambooFilter.h
)

target_include_directories(BambooFilter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} hash)
target_link_libraries(BambooFilter PRIVATE xxhash)