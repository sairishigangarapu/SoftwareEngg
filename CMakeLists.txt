cmake_minimum_required(VERSION 3.16)

project(UATC
    VERSION 1.0
    LANGUAGES CXX
)

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Filesystem library
add_library(uatc_filesystem
    src/filesystem/file_enumerator.cpp
    src/filesystem/file_reader.cpp
)

# Header include directory
target_include_directories(uatc_filesystem
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# Test executable
add_executable(uatc_tests
    tests/filesystem_test.cpp
)

# Link filesystem library to tests
target_link_libraries(uatc_tests
    PRIVATE
        uatc_filesystem
)

# Static MinGW runtime
# This avoids the Windows runtime DLL issue we encountered earlier.
if(MINGW)
    target_link_options(uatc_tests
        PRIVATE
            -static-libgcc
            -static-libstdc++
    )
endif()