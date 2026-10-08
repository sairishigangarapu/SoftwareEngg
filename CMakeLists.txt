cmake_minimum_required(VERSION 3.16)

project(UATC
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# Filesystem library
# ------------------------------------------------------------

add_library(uatc_filesystem
    src/filesystem/file_enumerator.cpp
    src/filesystem/file_reader.cpp
)

target_include_directories(uatc_filesystem
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# ------------------------------------------------------------
# Archive library
# ------------------------------------------------------------

add_library(uatc_archive
    src/archive/archive_binary.cpp
    src/archive/block.cpp
    src/archive/archive_writer.cpp
    src/archive/archive_reader.cpp
)

target_include_directories(uatc_archive
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# ------------------------------------------------------------
# Filesystem tests
# ------------------------------------------------------------

add_executable(uatc_tests
    tests/filesystem_test.cpp
)

target_link_libraries(uatc_tests
    PRIVATE
        uatc_filesystem
        uatc_archive
)

# ------------------------------------------------------------
# Archive binary tests
# ------------------------------------------------------------

add_executable(uatc_archive_tests
    tests/archive_binary_test.cpp
)

target_link_libraries(uatc_archive_tests
    PRIVATE
        uatc_archive
)

# ------------------------------------------------------------
# Block splitter tests
# ------------------------------------------------------------

add_executable(uatc_block_tests
    tests/block_test.cpp
)

target_link_libraries(uatc_block_tests
    PRIVATE
        uatc_archive
)

# ------------------------------------------------------------
# Archive writer tests
# ------------------------------------------------------------

add_executable(uatc_archive_writer_tests
    tests/archive_writer_test.cpp
)

target_link_libraries(uatc_archive_writer_tests
    PRIVATE
        uatc_filesystem
        uatc_archive
)

# ------------------------------------------------------------
# Archive reader tests
# ------------------------------------------------------------

add_executable(uatc_archive_reader_tests
    tests/archive_reader_test.cpp
)

target_link_libraries(uatc_archive_reader_tests
    PRIVATE
        uatc_filesystem
        uatc_archive
)

# ------------------------------------------------------------
# MinGW static runtime
# ------------------------------------------------------------

if(MINGW)

    target_link_options(uatc_tests
        PRIVATE
            -static-libgcc
            -static-libstdc++
    )

    target_link_options(uatc_archive_tests
        PRIVATE
            -static-libgcc
            -static-libstdc++
    )

    target_link_options(uatc_block_tests
        PRIVATE
            -static-libgcc
            -static-libstdc++
    )

    target_link_options(uatc_archive_writer_tests
        PRIVATE
            -static-libgcc
            -static-libstdc++
    )

    target_link_options(uatc_archive_reader_tests
        PRIVATE
            -static-libgcc
            -static-libstdc++
    )

endif()