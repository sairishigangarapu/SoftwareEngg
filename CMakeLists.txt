
cmake_minimum_required(VERSION 3.16)

project(UATC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

# --------------------------------------------------
# Core library
# --------------------------------------------------

add_library(uatc_core
    src/archive/archive_binary.cpp
    src/archive/archive_reader.cpp
    src/archive/archive_writer.cpp
    src/archive/block.cpp
    src/archive/codec.cpp
    src/archive/transforms.cpp
    src/filesystem/file_enumerator.cpp
    src/filesystem/file_reader.cpp
)

target_include_directories(uatc_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# --------------------------------------------------
# Tests
# --------------------------------------------------

if(BUILD_TESTING)

    # Filesystem tests
    add_executable(filesystem_test
        tests/filesystem_test.cpp
    )
    target_link_libraries(filesystem_test PRIVATE uatc_core)
    add_test(
        NAME filesystem_tests
        COMMAND filesystem_test
    )

    # Archive binary tests
    add_executable(archive_binary_test
        tests/archive_binary_test.cpp
    )
    target_link_libraries(archive_binary_test PRIVATE uatc_core)
    add_test(
        NAME archive_binary_tests
        COMMAND archive_binary_test
    )

    # Block tests
    add_executable(block_test
        tests/block_test.cpp
    )
    target_link_libraries(block_test PRIVATE uatc_core)
    add_test(
        NAME block_tests
        COMMAND block_test
    )

    # Archive writer tests
    add_executable(archive_writer_test
        tests/archive_writer_test.cpp
    )
    target_link_libraries(archive_writer_test PRIVATE uatc_core)
    add_test(
        NAME archive_writer_tests
        COMMAND archive_writer_test
    )

    # Archive reader tests
    add_executable(archive_reader_test
        tests/archive_reader_test.cpp
    )
    target_link_libraries(archive_reader_test PRIVATE uatc_core)
    add_test(
        NAME archive_reader_tests
        COMMAND archive_reader_test
    )

    # Transform and codec tests
    add_executable(transform_codec_test
        tests/transform_codec_test.cpp
    )
    target_link_libraries(transform_codec_test PRIVATE uatc_core)
    add_test(
        NAME transform_codec_tests
        COMMAND transform_codec_test
    )

    # Mixed raw/Delta transform tests
    add_executable(mixed_transform_test
        tests/mixed_transform_test.cpp
    )
    target_link_libraries(mixed_transform_test PRIVATE uatc_core)
    add_test(
        NAME mixed_transform_tests
        COMMAND mixed_transform_test
    )

    # All tests run from the project root so test_data paths work.
    set_tests_properties(
        filesystem_tests
        archive_binary_tests
        block_tests
        archive_writer_tests
        archive_reader_tests
        transform_codec_tests
        mixed_transform_tests
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )

endif()
