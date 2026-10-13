
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "archive/archive_binary.hpp"
#include "archive/archive_format.hpp"
#include "archive/archive_reader.hpp"
#include "archive/archive_writer.hpp"
#include "archive/block.hpp"
#include "filesystem/file_reader.hpp"

namespace {

namespace fs = std::filesystem;

bool expect_failure(
    const fs::path& archive_file,
    const std::string& expected_message
) {
    try {
        uatc::ArchiveReader::read(archive_file);

        std::cerr
            << "TEST FAILED: Invalid archive was accepted."
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
    const fs::path& source_file,
    const fs::path& archive_file,
    std::uint8_t codec_id,
    std::uint64_t stored_length,
    std::uint16_t transform_parameter = 0
) {
    const auto original_data =
        uatc::FileReader::read(source_file);

    const auto blocks =
        uatc::BlockSplitter::split(original_data, 4);

    uatc::ArchiveWriter::write(
        archive_file, source_file, blocks);

    std::uint64_t block_index_offset = 0;

    {
        uatc::BinaryReader reader(archive_file);

        reader.seekg(0);

        reader.read_u32(); // magic
        reader.read_u16(); // version
        reader.read_u16(); // header size
        reader.read_u32(); // file count
        reader.read_u32(); // block count

        const std::uint64_t file_table_offset =
            reader.read_u64();

        reader.read_u64(); // block index offset
        reader.read_u64(); // payload offset

        reader.seekg(file_table_offset);

        reader.read_u32(); // file ID
        reader.read_u64(); // path offset

        const std::uint32_t path_length =
            reader.read_u32();

        block_index_offset =
            file_table_offset +
            uatc::UATC_FILE_RECORD_SIZE +
            path_length;
    }

    // BlockRecord layout:
    // stored_length: bytes 24-31
    // transform_id:  byte 32
    // codec_id:      byte 33
    // parameter:     bytes 34-35
    const std::uint64_t stored_length_offset =
        block_index_offset + 24;

    const std::uint64_t codec_offset =
        block_index_offset + 33;

    const std::uint64_t parameter_offset =
        block_index_offset + 34;

    std::fstream file(
        archive_file,
        std::ios::binary |
        std::ios::in |
        std::ios::out
    );

    if (!file) {
        throw std::runtime_error(
            "Could not open archive for modification.");
    }

    file.seekp(
        static_cast<std::streamoff>(stored_length_offset),
        std::ios::beg);

    if (!file) {
        throw std::runtime_error(
            "Could not seek to stored_length.");
    }

    for (int i = 0; i < 8; ++i) {
        const std::uint8_t byte =
            static_cast<std::uint8_t>(
                (stored_length >> (i * 8)) & 0xFF);

        file.put(static_cast<char>(byte));
    }

    file.seekp(
        static_cast<std::streamoff>(codec_offset),
        std::ios::beg);

    if (!file) {
        throw std::runtime_error(
            "Could not seek to codec ID.");
    }

    file.put(static_cast<char>(codec_id));

    file.seekp(
        static_cast<std::streamoff>(parameter_offset),
        std::ios::beg);

    if (!file) {
        throw std::runtime_error(
            "Could not seek to transform parameter.");
    }

    // The archive format stores the uint16 parameter
    // in little-endian byte order.
    file.put(static_cast<char>(
        transform_parameter & 0xFF));

    file.put(static_cast<char>(
        (transform_parameter >> 8) & 0xFF));

    file.flush();

    if (!file) {
        throw std::runtime_error(
            "Failed to modify archive.");
    }
}

void remove_if_exists(const fs::path& path) {
    std::error_code error;
    fs::remove(path, error);
}

} // namespace

int main() {
    std::cout
        << "UATC archive reader tests started."
        << std::endl;

    const fs::path source_file =
        "test_data/hello.txt";

    const fs::path raw_archive =
        "hello.uatc";

    const fs::path invalid_codec_archive =
        "invalid_codec.uatc";

    const fs::path invalid_length_archive =
        "invalid_length.uatc";

    const fs::path invalid_parameter_archive =
        "invalid_parameter.uatc";

    try {
        const auto original_data =
            uatc::FileReader::read(source_file);

        const auto blocks =
            uatc::BlockSplitter::split(original_data, 4);

        // TEST 1: Raw archive round-trip.
        uatc::ArchiveWriter::write(
            raw_archive, source_file, blocks);

        const auto raw_result =
            uatc::ArchiveReader::read(raw_archive);

        if (raw_result != original_data) {
            std::cerr
                << "TEST FAILED: Raw archive round-trip."
                << std::endl;
            return 1;
        }

        std::cout
            << "TEST PASSED: Raw archive round-trip."
            << std::endl;

        // TEST 2: Extraction using every currently
        // implemented transform.
        const uatc::TransformId transforms[] = {
            uatc::TransformId::None,
            uatc::TransformId::RLE,
            uatc::TransformId::Delta
        };

        const char* transform_names[] = {
            "None",
            "RLE",
            "Delta"
        };

        for (std::size_t i = 0;
             i < sizeof(transforms) / sizeof(transforms[0]);
             ++i) {
            const fs::path archive =
                std::string("transform_") +
                transform_names[i] + ".uatc";

            uatc::ArchiveWriter::write(
                archive,
                source_file,
                blocks,
                transforms[i],
                uatc::CodecId::None);

            const auto extracted =
                uatc::ArchiveReader::read(archive);

            if (extracted != original_data) {
                std::cerr
                    << "TEST FAILED: Extraction with "
                    << transform_names[i]
                    << " transform."
                    << std::endl;

                remove_if_exists(archive);
                return 1;
            }

            std::cout
                << "TEST PASSED: Extraction with "
                << transform_names[i]
                << " transform."
                << std::endl;

            remove_if_exists(archive);
        }

        // TEST 3: Unsupported codec.
        // A four-byte first block has stored length 4.
        create_modified_archive(
            source_file,
            invalid_codec_archive,
            99,
            4);

        if (!expect_failure(
                invalid_codec_archive,
                "Unsupported codec")) {
            return 1;
        }

        std::cout
            << "TEST PASSED: Unsupported codec rejected."
            << std::endl;

        // TEST 4: Invalid raw stored length.
        create_modified_archive(
            source_file,
            invalid_length_archive,
            static_cast<std::uint8_t>(
                uatc::CodecId::None),
            999);

        if (!expect_failure(
                invalid_length_archive,
                "stored length")) {
            return 1;
        }

        std::cout
            << "TEST PASSED: Invalid stored length rejected."
            << std::endl;

        // TEST 5: A nonzero parameter is invalid for
        // the currently supported None transform.
        create_modified_archive(
            source_file,
            invalid_parameter_archive,
            static_cast<std::uint8_t>(
                uatc::CodecId::None),
            4,
            1);

        if (!expect_failure(
                invalid_parameter_archive,
                "Unsupported transform parameter")) {
            return 1;
        }

        std::cout
            << "TEST PASSED: Invalid transform parameter rejected."
            << std::endl;

        remove_if_exists(raw_archive);
        remove_if_exists(invalid_codec_archive);
        remove_if_exists(invalid_length_archive);
        remove_if_exists(invalid_parameter_archive);

        std::cout
            << "\nALL ARCHIVE READER TESTS PASSED!"
            << std::endl;

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr
            << "TEST FAILED: "
            << e.what()
            << std::endl;

        remove_if_exists(raw_archive);
        remove_if_exists(invalid_codec_archive);
        remove_if_exists(invalid_length_archive);
        remove_if_exists(invalid_parameter_archive);

        return 1;
    }
}
