
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
    src/core/block_stats.cpp
    src/core/block_slicer.cpp
    src/core/baseline_entropy_coder.cpp
    src/archive/archive_inspector.cpp
    src/archive/archive_verifier.cpp
)

target_include_directories(uatc_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
# --------------------------------------------------
# CLI executable
# --------------------------------------------------

add_executable(uatc
    src/cli/main.cpp
)

target_link_libraries(uatc PRIVATE uatc_core)

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
    # Archive inspector tests
    add_executable(archive_inspector_test
        tests/archive_inspector_test.cpp
    )
    target_link_libraries(archive_inspector_test PRIVATE uatc_core)
    add_test(
        NAME archive_inspector_tests
        COMMAND archive_inspector_test
    )
    # Archive verifier tests
    add_executable(archive_verifier_test
        tests/archive_verifier_test.cpp
    )
    target_link_libraries(archive_verifier_test PRIVATE uatc_core)
    add_test(
        NAME archive_verifier_tests
        COMMAND archive_verifier_test
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

    # Rishi's core tests (Days 1-5)
    add_executable(core_trivial_test
        tests/core/trivial_test.cpp
    )
    target_link_libraries(core_trivial_test PRIVATE uatc_core)
    add_test(NAME core_trivial_tests COMMAND core_trivial_test)

    add_executable(core_block_slice_test
        tests/core/block_slice_test.cpp
    )
    target_link_libraries(core_block_slice_test PRIVATE uatc_core)
    add_test(NAME core_block_slice_tests COMMAND core_block_slice_test)

    add_executable(core_transform_test
        tests/core/transform_test.cpp
    )
    target_link_libraries(core_transform_test PRIVATE uatc_core)
    add_test(NAME core_transform_tests COMMAND core_transform_test)

    add_executable(core_codec_test
        tests/core/codec_test.cpp
    )
    target_link_libraries(core_codec_test PRIVATE uatc_core)
    add_test(NAME core_codec_tests COMMAND core_codec_test)

    add_executable(core_pipeline_test
        tests/core/pipeline_test.cpp
    )
    target_link_libraries(core_pipeline_test PRIVATE uatc_core)
    add_test(NAME core_pipeline_tests COMMAND core_pipeline_test)

    # All tests run from the project root so test_data paths work.
    set_tests_properties(
        filesystem_tests
        archive_binary_tests
        block_tests
        archive_writer_tests
        archive_reader_tests
	archive_inspector_tests
	archive_verifier_tests
        transform_codec_tests
        mixed_transform_tests
        core_trivial_tests
        core_block_slice_tests
        core_transform_tests
        core_codec_tests
        core_pipeline_tests
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )

endif()
