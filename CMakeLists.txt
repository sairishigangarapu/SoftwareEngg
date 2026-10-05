cmake_minimum_required(VERSION 3.16)

project(UATC
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header files
include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

# UATC filesystem library
add_library(uatc_filesystem
    src/filesystem/file_enumerator.cpp
)

# Public headers
target_include_directories(uatc_filesystem
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# Test executable
add_executable(uatc_tests
    tests/filesystem_test.cpp
)

target_link_libraries(uatc_tests
    PRIVATE
        uatc_filesystem
)

# Use static C++ runtime libraries on MinGW/Windows
target_link_options(uatc_tests
    PRIVATE
        -static-libgcc
        -static-libstdc++
)