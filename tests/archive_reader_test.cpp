#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "archive/archive_binary.hpp"
#include "archive/archive_reader.hpp"
#include "archive/archive_writer.hpp"
#include "archive/block.hpp"
#include "filesystem/file_reader.hpp"

namespace {

bool expect_failure(
    const std::filesystem::path& archive_file,
    const std::string& expected_message
) {
    try {
        uatc::ArchiveReader::read(archive_file);

        std::cerr
            << "TEST FAILED: ArchiveReader accepted "
            << "an invalid archive."
            << std::endl;

        return false;
    }
    catch (const std::exception& e) {
        const std::string message = e.what();

        if (message.find(expected_message) ==
            std::string::npos) {

            std::cerr
                << "TEST FAILED: Unexpected error message: "
                << message
                << std::endl;

            return false;
        }

        return true;
    }
}

void create_modified_archive(
    const std::filesystem::path& source_file,
    const std::filesystem::path& archive_file,
    std::uint8_t codec_id,
    std::uint64_t stored_length
) {
    // --------------------------------------------------------
    // Create a normal archive
    // --------------------------------------------------------

    const auto original_data =
        uatc::FileReader::read(source_file);

    const auto blocks =
        uatc::BlockSplitter::split(
            original_data,
            4
        );

    uatc::ArchiveWriter::write(
        archive_file,
        source_file,
        blocks
    );

    // --------------------------------------------------------
    // First determine the block index offset.
    //
    // Keep BinaryReader inside this scope so that it is
    // destroyed before we reopen the archive for writing.
    // --------------------------------------------------------

    std::uint64_t block_index_offset = 0;

    {
        uatc::BinaryReader reader(
            archive_file
        );

        reader.seekg(0);

        // Header
        reader.read_u32(); // magic
        reader.read_u16(); // version
        reader.read_u16(); // header size
        reader.read_u32(); // file count
        reader.read_u32(); // block count

        const std::uint64_t file_table_offset =
            reader.read_u64();

        reader.read_u64(); // block index offset
        reader.read_u64(); // payload offset

        // File table
        reader.seekg(
            file_table_offset
        );

        reader.read_u32(); // file ID
        reader.read_u64(); // path offset

        const std::uint32_t path_length =
            reader.read_u32();

        // File record + path
        block_index_offset =
            file_table_offset +
            uatc::UATC_FILE_RECORD_SIZE +
            path_length;
    }

    // --------------------------------------------------------
    // Block record layout
    //
    // file_id          4
    // block_id         4
    // payload_offset   8
    // original_length  8
    // stored_length    8
    // transform_id     1
    // codec_id         1
    // reserved         2
    // checksum         4
    //
    // stored_length starts at offset 24.
    // transform_id starts at offset 32.
    // codec_id starts at offset 33.
    // --------------------------------------------------------

    const std::uint64_t stored_length_offset =
        block_index_offset + 24;

    const std::uint64_t codec_offset =
        block_index_offset + 33;

    // --------------------------------------------------------
    // Modify archive
    // --------------------------------------------------------

    std::fstream file(
        archive_file,
        std::ios::binary |
        std::ios::in |
        std::ios::out
    );

    if (!file) {
        throw std::runtime_error(
            "Could not open archive for modification."
        );
    }

    // --------------------------------------------------------
    // Modify stored_length
    // --------------------------------------------------------

    file.seekp(
        static_cast<std::streamoff>(
            stored_length_offset
        ),
        std::ios::beg
    );

    if (!file) {
        throw std::runtime_error(
            "Could not seek to stored_length."
        );
    }

    for (int i = 0; i < 8; ++i) {
        const std::uint8_t byte =
            static_cast<std::uint8_t>(
                (stored_length >> (i * 8)) & 0xFF
            );

        file.put(
            static_cast<char>(byte)
        );
    }

    // --------------------------------------------------------
    // Modify codec ID
    // --------------------------------------------------------

    file.seekp(
        static_cast<std::streamoff>(
            codec_offset
        ),
        std::ios::beg
    );

    if (!file) {
        throw std::runtime_error(
            "Could not seek to codec ID."
        );
    }

    file.put(
        static_cast<char>(codec_id)
    );

    file.flush();

    if (!file) {
        throw std::runtime_error(
            "Failed to modify archive."
        );
    }

    file.close();
}

}

int main() {

    const std::filesystem::path source_file =
        "test_data/hello.txt";

    const std::filesystem::path archive_file =
        "test_data/hello.uatc";

    const std::filesystem::path invalid_codec_archive =
        "test_data/invalid_codec.uatc";

    const std::filesystem::path invalid_length_archive =
        "test_data/invalid_length.uatc";

    std::cout
        << "UATC archive reader Day 5 tests started."
        << std::endl;

    try {

        // ====================================================
        // TEST 1: Normal raw archive round-trip
        // ====================================================

        const auto original_data =
            uatc::FileReader::read(
                source_file
            );

        const auto blocks =
            uatc::BlockSplitter::split(
                original_data,
                4
            );

        uatc::ArchiveWriter::write(
            archive_file,
            source_file,
            blocks
        );

        const auto reconstructed_data =
            uatc::ArchiveReader::read(
                archive_file
            );

        if (reconstructed_data != original_data) {
            std::cerr
                << "TEST FAILED: Raw archive round-trip "
                << "does not match."
                << std::endl;

            return 1;
        }

        std::cout
            << "TEST PASSED: Raw archive round-trip."
            << std::endl;

        // ====================================================
        // TEST 2: Unsupported codec
        // ====================================================

        create_modified_archive(
            source_file,
            invalid_codec_archive,
            99,
            4
        );

        if (!expect_failure(
                invalid_codec_archive,
                "Unsupported codec")) {

            return 1;
        }

        std::cout
            << "TEST PASSED: Unsupported codec was rejected."
            << std::endl;

        // ====================================================
        // TEST 3: Invalid stored length
        // ====================================================

        create_modified_archive(
            source_file,
            invalid_length_archive,
            0,
            999
        );

        if (!expect_failure(
                invalid_length_archive,
                "Raw block stored length")) {

            return 1;
        }

        std::cout
            << "TEST PASSED: Invalid stored length "
            << "was rejected."
            << std::endl;

        // ====================================================
        // Remove generated files
        // ====================================================

        std::filesystem::remove(
            archive_file
        );

        std::filesystem::remove(
            invalid_codec_archive
        );

        std::filesystem::remove(
            invalid_length_archive
        );

        // ====================================================
        // Final result
        // ====================================================

        std::cout
            << "\nALL DAY 5 ARCHIVE READER TESTS PASSED!"
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