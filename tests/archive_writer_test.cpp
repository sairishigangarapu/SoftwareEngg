
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

#include "archive/archive_writer.hpp"
#include "archive/archive_reader.hpp"
#include "archive/block.hpp"
#include "filesystem/file_reader.hpp"

namespace {

std::vector<std::uint8_t> read_binary_file(
    const std::filesystem::path& path
) {
    std::ifstream input(path, std::ios::binary);

    if (!input) {
        throw std::runtime_error(
            "Could not read file: " + path.string()
        );
    }

    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    );
}

} // namespace

int main() {
    const std::filesystem::path source_file =
        "test_data/hello.txt";

    const std::filesystem::path archive_file =
        "test_data/hello.uatc";

    const std::filesystem::path reordered_archive_file =
        "test_data/hello_reordered.uatc";

    constexpr std::size_t block_size = 4;

    std::cout << "UATC archive writer test started."
              << std::endl;

    try {
        // TEST 1: Read source file.
        const auto file_data =
            uatc::FileReader::read(source_file);

        if (file_data.empty()) {
            throw std::runtime_error("Source file is empty.");
        }

        std::cout << "TEST PASSED: Source file was read."
                  << std::endl;

        // TEST 2: Split the source data into blocks.
        const auto blocks =
            uatc::BlockSplitter::split(
                file_data,
                block_size
            );

        if (blocks.empty()) {
            throw std::runtime_error("No blocks were created.");
        }

        std::cout << "TEST PASSED: Source data was split into "
                  << blocks.size() << " blocks."
                  << std::endl;

        // TEST 3: Create the normal archive.
        uatc::ArchiveWriter::write(
            archive_file,
            source_file,
            blocks
        );

        if (!std::filesystem::exists(archive_file)) {
            throw std::runtime_error("Archive was not created.");
        }

        const auto archive_size =
            std::filesystem::file_size(archive_file);

        if (archive_size == 0) {
            throw std::runtime_error("Archive is empty.");
        }

        std::cout << "TEST PASSED: Archive was created ("
                  << archive_size << " bytes)."
                  << std::endl;

        // TEST 4: Reverse block order to simulate
        // processing results arriving out of order.
        auto reordered_blocks = blocks;

        std::reverse(
            reordered_blocks.begin(),
            reordered_blocks.end()
        );

        uatc::ArchiveWriter::write(
            reordered_archive_file,
            source_file,
            reordered_blocks
        );

        // TEST 5: Archives must be byte-for-byte identical.
        const auto normal_archive_bytes =
            read_binary_file(archive_file);

        const auto reordered_archive_bytes =
            read_binary_file(reordered_archive_file);

        if (normal_archive_bytes != reordered_archive_bytes) {
            throw std::runtime_error(
                "Archive output changed when block order changed."
            );
        }

        std::cout
            << "TEST PASSED: Archive output is deterministic "
            << "for reordered blocks."
            << std::endl;

        // TEST 6: Read the reordered archive and verify
        // the reconstructed bytes match the original file.
        const auto restored_data =
            uatc::ArchiveReader::read(
                reordered_archive_file
            );

        if (restored_data != file_data) {
            throw std::runtime_error(
                "Reordered archive did not restore original bytes."
            );
        }

        std::cout
            << "TEST PASSED: Reordered archive restored correctly."
            << std::endl;

        // Remove only the generated archives.
        std::filesystem::remove(archive_file);
        std::filesystem::remove(reordered_archive_file);

        std::cout
            << "\nALL ARCHIVE WRITER TESTS PASSED!"
            << std::endl;

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "\nTEST FAILED: "
                  << error.what() << std::endl;

        std::error_code cleanup_error;

        std::filesystem::remove(
            archive_file,
            cleanup_error
        );

        std::filesystem::remove(
            reordered_archive_file,
            cleanup_error
        );

        return 1;
    }
}
