
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

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
        << "UATC archive writer test started."
        << std::endl;

    try {
        // TEST 1: Read source file
        const auto file_data =
            uatc::FileReader::read(source_file);

        if (file_data.empty()) {
            throw std::runtime_error("Source file is empty.");
        }

        std::cout
            << "TEST PASSED: Source file was read."
            << std::endl;

        // TEST 2: Split file into blocks
        const auto blocks =
            uatc::BlockSplitter::split(
                file_data,
                block_size
            );

        if (blocks.empty()) {
            throw std::runtime_error("No blocks were created.");
        }

        std::cout
            << "TEST PASSED: Source data was split into "
            << blocks.size()
            << " blocks."
            << std::endl;

        // TEST 3: Create archive
        uatc::ArchiveWriter::write(
            archive_file,
            source_file,
            blocks
        );

        if (!std::filesystem::exists(archive_file)) {
            throw std::runtime_error("Archive was not created.");
        }

        std::cout
            << "TEST PASSED: .uatc archive was created."
            << std::endl;

        // TEST 4: Check archive size
        const auto archive_size =
            std::filesystem::file_size(archive_file);

        if (archive_size == 0) {
            throw std::runtime_error("Archive is empty.");
        }

        std::cout
            << "TEST PASSED: Archive contains "
            << archive_size
            << " bytes."
            << std::endl;

        std::cout
            << "\nALL ARCHIVE WRITER TESTS PASSED!"
            << std::endl;

        // Remove the generated archive, not the source file
        std::filesystem::remove(archive_file);

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\nTEST FAILED: " << e.what() << std::endl;

        // Clean up generated output even if a test fails
        std::error_code cleanup_error;
        std::filesystem::remove(archive_file, cleanup_error);

        return 1;
    }
}
