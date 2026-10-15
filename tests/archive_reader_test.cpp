
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

        std::cerr << "TEST FAILED: Invalid archive was accepted.\n";
        return false;
    } catch (const std::exception& e) {
        const std::string message = e.what();

        if (message.find(expected_message) == std::string::npos) {
            std::cerr << "TEST FAILED: Unexpected error message: "
                      << message << '\n';
            return false;
        }

        return true;
    }
}

void write_u64_at(
    const fs::path& archive_file,
    std::uint64_t position,
    std::uint64_t value
) {
    std::fstream file(
        archive_file,
        std::ios::binary | std::ios::in | std::ios::out
    );

    if (!file) {
        throw std::runtime_error("Could not open archive for modification.");
    }

    file.seekp(static_cast<std::streamoff>(position));

    if (!file) {
        throw std::runtime_error("Could not seek to archive field.");
    }

    for (int i = 0; i < 8; ++i) {
        file.put(static_cast<char>((value >> (i * 8)) & 0xFF));
    }

    file.flush();

    if (!file) {
        throw std::runtime_error("Failed to modify archive field.");
    }
}

std::uint64_t get_first_block_index_offset(
    const fs::path& archive_file
) {
    uatc::BinaryReader reader(archive_file);

    reader.seekg(0);
    reader.read_u32(); // magic
    reader.read_u16(); // version
    reader.read_u16(); // header size
    reader.read_u32(); // file count
    reader.read_u32(); // block count

    reader.read_u64(); // file table offset
    return reader.read_u64(); // block index offset
}

std::uint64_t get_first_payload_offset(
    const fs::path& archive_file
) {
    uatc::BinaryReader reader(archive_file);

    reader.seekg(0);
    reader.read_u32(); // magic
    reader.read_u16(); // version
    reader.read_u16(); // header size
    reader.read_u32(); // file count
    reader.read_u32(); // block count
    reader.read_u64(); // file table offset
    reader.read_u64(); // block index offset

    return reader.read_u64(); // payload offset
}

void create_modified_archive(
    const fs::path& source_file,
    const fs::path& archive_file,
    std::uint8_t codec_id,
    std::uint64_t stored_length,
    std::uint16_t transform_parameter = 0
) {
    const auto original_data = uatc::FileReader::read(source_file);
    const auto blocks = uatc::BlockSplitter::split(original_data, 4);

    uatc::ArchiveWriter::write(archive_file, source_file, blocks);

    std::uint64_t block_index_offset = 0;

    {
        uatc::BinaryReader reader(archive_file);

        reader.seekg(0);
        reader.read_u32();
        reader.read_u16();
        reader.read_u16();
        reader.read_u32();
        reader.read_u32();

        const std::uint64_t file_table_offset = reader.read_u64();
        reader.read_u64();
        reader.read_u64();

        reader.seekg(file_table_offset);
        reader.read_u32();
        reader.read_u64();

        const std::uint32_t path_length = reader.read_u32();

        block_index_offset =
            file_table_offset +
            uatc::UATC_FILE_RECORD_SIZE +
            path_length;
    }

    const std::uint64_t stored_length_offset = block_index_offset + 24;
    const std::uint64_t codec_offset = block_index_offset + 33;
    const std::uint64_t parameter_offset = block_index_offset + 34;

    std::fstream file(
        archive_file,
        std::ios::binary | std::ios::in | std::ios::out
    );

    if (!file) {
        throw std::runtime_error("Could not open archive for modification.");
    }

    file.seekp(static_cast<std::streamoff>(stored_length_offset));

    if (!file) {
        throw std::runtime_error("Could not seek to stored_length.");
    }

    for (int i = 0; i < 8; ++i) {
        file.put(static_cast<char>((stored_length >> (i * 8)) & 0xFF));
    }

    file.seekp(static_cast<std::streamoff>(codec_offset));

    if (!file) {
        throw std::runtime_error("Could not seek to codec ID.");
    }

    file.put(static_cast<char>(codec_id));

    file.seekp(static_cast<std::streamoff>(parameter_offset));

    if (!file) {
        throw std::runtime_error("Could not seek to transform parameter.");
    }

    file.put(static_cast<char>(transform_parameter & 0xFF));
    file.put(static_cast<char>((transform_parameter >> 8) & 0xFF));
    file.flush();

    if (!file) {
        throw std::runtime_error("Failed to modify archive.");
    }
}

void corrupt_first_payload_offset(const fs::path& archive_file) {
    const std::uint64_t block_index_offset =
        get_first_block_index_offset(archive_file);

    // The first block record's payload offset begins at byte 8.
    const std::uint64_t payload_offset_position =
        block_index_offset + 8;

    // Deliberately impossible offset: 0x7FFFFFFFFFFFFFFF.
    write_u64_at(
        archive_file,
        payload_offset_position,
        0x7FFFFFFFFFFFFFFFULL
    );
}

void corrupt_first_payload_byte(const fs::path& archive_file) {
    const std::uint64_t payload_offset =
        get_first_payload_offset(archive_file);

    std::fstream file(
        archive_file,
        std::ios::binary | std::ios::in | std::ios::out
    );

    if (!file) {
        throw std::runtime_error(
            "Could not open archive to corrupt payload."
        );
    }

    file.seekp(static_cast<std::streamoff>(payload_offset));

    if (!file) {
        throw std::runtime_error("Could not seek to first payload byte.");
    }

    // Baseline control 5 means six literal bytes should follow.
    // The test sets stored_length to 1, so the literal run is truncated.
    file.put(static_cast<char>(5));
    file.flush();

    if (!file) {
        throw std::runtime_error("Failed to corrupt payload.");
    }
}

void remove_if_exists(const fs::path& path) {
    std::error_code error;
    fs::remove(path, error);
}

} // namespace

int main() {
    std::cout << "UATC archive reader tests started.\n";

    const fs::path source_file = "test_data/hello.txt";
    const fs::path raw_archive = "hello.uatc";
    const fs::path invalid_codec_archive = "invalid_codec.uatc";
    const fs::path invalid_length_archive = "invalid_length.uatc";
    const fs::path invalid_parameter_archive = "invalid_parameter.uatc";
    const fs::path invalid_offset_archive = "invalid_offset.uatc";
    const fs::path malformed_baseline_archive = "malformed_baseline.uatc";

    try {
        const auto original_data = uatc::FileReader::read(source_file);
        const auto blocks = uatc::BlockSplitter::split(original_data, 4);

        // TEST 1: Raw archive round-trip.
        uatc::ArchiveWriter::write(raw_archive, source_file, blocks);

        if (uatc::ArchiveReader::read(raw_archive) != original_data) {
            throw std::runtime_error("Raw archive round-trip failed.");
        }

        std::cout << "TEST PASSED: Raw archive round-trip.\n";

        // TEST 2: Extraction using every implemented transform.
        const uatc::TransformId transforms[] = {
            uatc::TransformId::None,
            uatc::TransformId::RLE,
            uatc::TransformId::Delta
        };

        const char* transform_names[] = {"None", "RLE", "Delta"};

        for (std::size_t i = 0;
             i < sizeof(transforms) / sizeof(transforms[0]);
             ++i) {
            const fs::path archive =
                std::string("transform_") + transform_names[i] + ".uatc";

            uatc::ArchiveWriter::write(
                archive,
                source_file,
                blocks,
                transforms[i],
                uatc::CodecId::None
            );

            if (uatc::ArchiveReader::read(archive) != original_data) {
                remove_if_exists(archive);
                throw std::runtime_error(
                    std::string("Extraction failed for transform ") +
                    transform_names[i]
                );
            }

            std::cout << "TEST PASSED: Extraction with "
                      << transform_names[i] << " transform.\n";

            remove_if_exists(archive);
        }

        // TEST 3: Unsupported codec.
        create_modified_archive(
            source_file, invalid_codec_archive, 99, 4
        );

        if (!expect_failure(invalid_codec_archive, "Unsupported codec")) {
            throw std::runtime_error("Unsupported codec was not rejected.");
        }

        std::cout << "TEST PASSED: Unsupported codec rejected.\n";

        // TEST 4: Invalid raw stored length.
        create_modified_archive(
            source_file,
            invalid_length_archive,
            static_cast<std::uint8_t>(uatc::CodecId::None),
            999
        );

        if (!expect_failure(invalid_length_archive, "stored length")) {
            throw std::runtime_error("Invalid stored length was not rejected.");
        }

        std::cout << "TEST PASSED: Invalid stored length rejected.\n";

        // TEST 5: Unsupported transform parameter.
        create_modified_archive(
            source_file,
            invalid_parameter_archive,
            static_cast<std::uint8_t>(uatc::CodecId::None),
            4,
            1
        );

        if (!expect_failure(
                invalid_parameter_archive,
                "Unsupported transform parameter")) {
            throw std::runtime_error(
                "Invalid transform parameter was not rejected."
            );
        }

        std::cout << "TEST PASSED: Invalid transform parameter rejected.\n";

        // TEST 6: Payload offset beyond archive boundaries.
        uatc::ArchiveWriter::write(
            invalid_offset_archive, source_file, blocks
        );

        corrupt_first_payload_offset(invalid_offset_archive);

        if (!expect_failure(
                invalid_offset_archive,
                "Block payload extends beyond archive")) {
            throw std::runtime_error(
                "Invalid payload offset was not rejected."
            );
        }

        std::cout << "TEST PASSED: Invalid payload offset rejected.\n";

        // TEST 7: Malformed Baseline codec payload.
        create_modified_archive(
            source_file,
            malformed_baseline_archive,
            static_cast<std::uint8_t>(uatc::CodecId::Baseline),
            1
        );

        corrupt_first_payload_byte(malformed_baseline_archive);

        if (!expect_failure(
                malformed_baseline_archive,
                "Truncated Baseline literal run")) {
            throw std::runtime_error(
                "Malformed Baseline payload was not rejected."
            );
        }

        std::cout << "TEST PASSED: Malformed Baseline payload rejected.\n";

        remove_if_exists(raw_archive);
        remove_if_exists(invalid_codec_archive);
        remove_if_exists(invalid_length_archive);
        remove_if_exists(invalid_parameter_archive);
        remove_if_exists(invalid_offset_archive);
        remove_if_exists(malformed_baseline_archive);

        std::cout << "\nALL ARCHIVE READER TESTS PASSED!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "TEST FAILED: " << e.what() << '\n';

        remove_if_exists(raw_archive);
        remove_if_exists(invalid_codec_archive);
        remove_if_exists(invalid_length_archive);
        remove_if_exists(invalid_parameter_archive);
        remove_if_exists(invalid_offset_archive);
        remove_if_exists(malformed_baseline_archive);

        return 1;
    }
}
