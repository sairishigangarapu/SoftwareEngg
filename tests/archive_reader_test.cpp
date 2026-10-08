#include <cstdint>
#include <filesystem>
#include <iostream>
#include <vector>

#include "archive/archive_reader.hpp"
#include "archive/archive_writer.hpp"
#include "archive/block.hpp"
#include "filesystem/file_reader.hpp"

int main() {
    const std::filesystem::path source_file =
        "test_data/hello.txt";

    const std::filesystem::path archive_file =
        "test_data/hello.uatc";

    const std::size_t block_size = 4;

    std::cout
        << "UATC archive reader test started."
        << std::endl;

    try {
        // ----------------------------------------------------
        // TEST 1: Read original file
        // ----------------------------------------------------

        const auto original_data =
            uatc::FileReader::read(source_file);

        if (original_data.empty()) {
            std::cerr
                << "TEST FAILED: Source file is empty."
                << std::endl;

            return 1;
        }

        std::cout
            << "TEST PASSED: Original file was read."
            << std::endl;

        // ----------------------------------------------------
        // TEST 2: Split original file into blocks
        // ----------------------------------------------------

        const auto blocks =
            uatc::BlockSplitter::split(
                original_data,
                block_size
            );

        if (blocks.empty()) {
            std::cerr
                << "TEST FAILED: No blocks were created."
                << std::endl;

            return 1;
        }

        std::cout
            << "TEST PASSED: Source data was split into "
            << blocks.size()
            << " blocks."
            << std::endl;

        // ----------------------------------------------------
        // TEST 3: Write archive
        // ----------------------------------------------------

        uatc::ArchiveWriter::write(
            archive_file,
            source_file,
            blocks
        );

        if (!std::filesystem::exists(archive_file)) {
            std::cerr
                << "TEST FAILED: Archive was not created."
                << std::endl;

            return 1;
        }

        std::cout
            << "TEST PASSED: Archive was created."
            << std::endl;

        // ----------------------------------------------------
        // TEST 4: Read archive
        // ----------------------------------------------------

        const auto reconstructed_data =
            uatc::ArchiveReader::read(
                archive_file
            );

        std::cout
            << "TEST PASSED: Archive was read."
            << std::endl;

        // ----------------------------------------------------
        // TEST 5: Compare sizes
        // ----------------------------------------------------

        if (reconstructed_data.size() !=
            original_data.size()) {

            std::cerr
                << "TEST FAILED: Reconstructed size "
                << reconstructed_data.size()
                << " does not match original size "
                << original_data.size()
                << "."
                << std::endl;

            return 1;
        }

        std::cout
            << "TEST PASSED: Reconstructed size matches."
            << std::endl;

        // ----------------------------------------------------
        // TEST 6: Compare bytes
        // ----------------------------------------------------

        if (reconstructed_data != original_data) {

            std::cerr
                << "TEST FAILED: Reconstructed data "
                << "does not match original data."
                << std::endl;

            return 1;
        }

        std::cout
            << "TEST PASSED: Reconstructed data "
            << "matches original data."
            << std::endl;

        // ----------------------------------------------------
        // Final result
        // ----------------------------------------------------

        std::cout
            << "\nALL ARCHIVE READER TESTS PASSED!"
            << std::endl;

        return 0;
    }
    catch (const std::exception& e) {

        std::cerr
            << "\nTEST FAILED: "
            << e.what()
            << std::endl;

        return 1;
    }
}