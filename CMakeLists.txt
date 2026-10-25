
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
    src/core/cost_estimator.cpp
    src/core/delta_transform.cpp
    src/core/xor_transform.cpp
    src/core/byte_shuffle_transform.cpp
    src/core/rle_transform.cpp
    src/core/transform_factory.cpp
    src/core/compressibility_probe.cpp
    src/core/transform_selector.cpp
    src/core/compressor.cpp
    src/core/error.cpp
    src/core/planner.cpp

    src/archive/archive_inspector.cpp
    src/archive/archive_verifier.cpp
)

target_include_directories(uatc_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# --------------------------------------------------
# CLI applications
# --------------------------------------------------

add_executable(uatc
    src/cli/main.cpp
)

target_link_libraries(uatc PRIVATE uatc_core)

# Preserve the existing CLI while integrating the team's CLI.
add_executable(uatc_cli
    src/main.cpp
)

target_link_libraries(uatc_cli PRIVATE uatc_core)

# --------------------------------------------------
# Tests
# --------------------------------------------------

if(BUILD_TESTING)

    # Existing filesystem and archive tests
    add_executable(filesystem_test tests/filesystem_test.cpp)
    target_link_libraries(filesystem_test PRIVATE uatc_core)
    add_test(NAME filesystem_tests COMMAND filesystem_test)

    add_executable(archive_binary_test tests/archive_binary_test.cpp)
    target_link_libraries(archive_binary_test PRIVATE uatc_core)
    add_test(NAME archive_binary_tests COMMAND archive_binary_test)

    add_executable(block_test tests/block_test.cpp)
    target_link_libraries(block_test PRIVATE uatc_core)
    add_test(NAME block_tests COMMAND block_test)

    add_executable(archive_writer_test tests/archive_writer_test.cpp)
    target_link_libraries(archive_writer_test PRIVATE uatc_core)
    add_test(NAME archive_writer_tests COMMAND archive_writer_test)

    add_executable(archive_reader_test tests/archive_reader_test.cpp)
    target_link_libraries(archive_reader_test PRIVATE uatc_core)
    add_test(NAME archive_reader_tests COMMAND archive_reader_test)

    add_executable(archive_inspector_test
        tests/archive_inspector_test.cpp
    )
    target_link_libraries(archive_inspector_test PRIVATE uatc_core)
    add_test(NAME archive_inspector_tests COMMAND archive_inspector_test)

    add_executable(archive_verifier_test
        tests/archive_verifier_test.cpp
    )
    target_link_libraries(archive_verifier_test PRIVATE uatc_core)
    add_test(NAME archive_verifier_tests COMMAND archive_verifier_test)

    add_executable(transform_codec_test
        tests/transform_codec_test.cpp
    )
    target_link_libraries(transform_codec_test PRIVATE uatc_core)
    add_test(NAME transform_codec_tests COMMAND transform_codec_test)

    add_executable(mixed_transform_test
        tests/mixed_transform_test.cpp
    )
    target_link_libraries(mixed_transform_test PRIVATE uatc_core)
    add_test(NAME mixed_transform_tests COMMAND mixed_transform_test)

    # Your path traversal test
    add_executable(path_traversal_test
        tests/path_traversal_test.cpp
    )
    target_link_libraries(path_traversal_test PRIVATE uatc_core)
    add_test(NAME path_traversal_tests COMMAND path_traversal_test)

    # Core tests: Days 1-5
    add_executable(core_trivial_test tests/core/trivial_test.cpp)
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

    add_executable(core_codec_test tests/core/codec_test.cpp)
    target_link_libraries(core_codec_test PRIVATE uatc_core)
    add_test(NAME core_codec_tests COMMAND core_codec_test)

    add_executable(core_pipeline_test tests/core/pipeline_test.cpp)
    target_link_libraries(core_pipeline_test PRIVATE uatc_core)
    add_test(NAME core_pipeline_tests COMMAND core_pipeline_test)

    # Core tests: Days 6-9
    add_executable(core_transforms_extended_test
        tests/core/transforms_extended_test.cpp
    )
    target_link_libraries(core_transforms_extended_test PRIVATE uatc_core)
    add_test(
        NAME core_transforms_extended_tests
        COMMAND core_transforms_extended_test
    )

    add_executable(core_probe_test tests/core/probe_test.cpp)
    target_link_libraries(core_probe_test PRIVATE uatc_core)
    add_test(NAME core_probe_tests COMMAND core_probe_test)

    add_executable(core_selector_test tests/core/selector_test.cpp)
    target_link_libraries(core_selector_test PRIVATE uatc_core)
    add_test(NAME core_selector_tests COMMAND core_selector_test)

    add_executable(core_archive_integration_test
        tests/core/archive_integration_test.cpp
    )
    target_link_libraries(core_archive_integration_test PRIVATE uatc_core)
    add_test(
        NAME core_archive_integration_tests
        COMMAND core_archive_integration_test
    )

    add_executable(core_compressor_test
        tests/core/compressor_test.cpp
    )
    target_link_libraries(core_compressor_test PRIVATE uatc_core)
    add_test(NAME core_compressor_tests COMMAND core_compressor_test)

    add_executable(core_planner_test tests/core/planner_test.cpp)
    target_link_libraries(core_planner_test PRIVATE uatc_core)
    add_test(NAME core_planner_tests COMMAND core_planner_test)

    # End-to-end round-trip test
    add_executable(integration_roundtrip_test
        tests/integration_roundtrip_test.cpp
    )
    target_link_libraries(integration_roundtrip_test PRIVATE uatc_core)
    add_test(
        NAME integration_roundtrip_tests
        COMMAND integration_roundtrip_test
    )

    # Run tests from the project root so relative test_data paths work.
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
        path_traversal_tests
        core_trivial_tests
        core_block_slice_tests
        core_transform_tests
        core_codec_tests
        core_pipeline_tests
        core_transforms_extended_tests
        core_probe_tests
        core_selector_tests
        core_archive_integration_tests
        core_compressor_tests
        core_planner_tests
        integration_roundtrip_tests
        PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )

endif()
