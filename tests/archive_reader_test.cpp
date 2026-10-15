
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
        reader.read_u32(); // magic
        reader.read_u16(); // version
        reader.read_u16(); // header size
        reader.read_u32(); // file count
        reader.read_u32(); // block count
        reader.read_u64(); // file table offset

        block_index_offset = reader.read_u64();
    }

    const std::uint64_t stored_length_offset =
        block_index_offset + 24;

    const std::uint64_t codec_offset =
        block_index_offset + 33;

    const std::uint64_t parameter_offset =
        block_index_offset + 34;

    std::fstream file(
        archive_file,
        std::ios::binary | std::ios::in | std::ios::out
    );

    if (!file) {
        throw std::runtime_error(
            "Could not open archive for modification."
        );
    }

    file.seekp(static_cast<std::streamoff>(stored_length_offset));

    if (!file) {
        throw std::runtime_error("Could not seek to stored_length.");
    }

    for (int i = 0; i < 8; ++i) {
        const std::uint8_t byte = static_cast<std::uint8_t>(
            (stored_length >> (i * 8)) & 0xFF
        );

        file.put(static_cast<char>(byte));
    }

    file.seekp(static_cast<std::streamoff>(codec_offset));

    if (!file) {
        throw std::runtime_error("Could not seek to codec ID.");
    }

    file.put(static_cast<char>(codec_id));

    file.seekp(static_cast<std::streamoff>(parameter_offset));

    if (!file) {
        throw std::runtime_error(
            "Could not seek to transform parameter."
        );
    }

    file.put(static_cast<char>(transform_parameter & 0xFF));
    file.put(static_cast<char>((transform_parameter >> 8) & 0xFF));
    file.flush();

    if (!file) {
        throw std::runtime_error("Failed to modify archive.");
    }
}

void corrupt_first_payload_offset(
    const fs::path& archive_file
) {
    std::uint64_t block_index_offset = 0;

    {
        uatc::BinaryReader reader(archive_file);

        reader.seekg(0);
        reader.read_u32(); // magic
        reader.read_u16(); // version
        reader.read_u16(); // header size
        reader.read_u32(); // file count
        reader.read_u32(); // block count
        reader.read_u64(); // file table offset

        block_index_offset = reader.read_u64();
    }

    // The first block record's payload offset begins at byte 8.
    const std::uint64_t payload_offset_position =
        block_index_offset + 8;

    std::fstream file(
        archive_file,
        std::ios::binary | std::ios::in | std::ios::out
    );

    if (!file) {
        throw std::runtime_error(
            "Could not open archive to corrupt payload offset."
        );
    }

    file.seekp(
        static_cast<std::streamoff>(payload_offset_position)
    );

    if (!file) {
        throw std::runtime_error("Could not seek to payload offset.");
    }

    const std::uint64_t invalid_offset = 0x7FFFFFFFFFFFFFFFULL;

    for (int i = 0; i < 8; ++i) {
        file.put(static_cast<char>(
            (invalid_offset >> (i * 8)) & 0xFF
        ));
    }

    file.flush();

    if (!file) {
        throw std::runtime_error("Failed to corrupt payload offset.");
    }
}

void corrupt_first_payload_byte(
    const fs::path& archive_file
) {
    std::uint64_t block_index_offset = 0;
    std::uint64_t payload_offset = 0;

    {
        uatc::BinaryReader reader(archive_file);

        reader.seekg(0);
        reader.read_u32(); // magic
        reader.read_u16(); // version
        reader.read_u16(); // header size
        reader.read_u32(); // file count
        reader.read_u32(); // block count
        reader.read_u64(); // file table offset

        block_index_offset = reader.read_u64();

        reader.seekg(block_index_offset);
        reader.read_u32(); // block file ID
        reader.read_u32(); // block ID

        payload_offset = reader.read_u64();
    }

    std::fstream file(
        archive_file,
        std::ios::binary | std::ios::in | std::ios::out
    );

    if (!file) {
        throw std::runtime_error(
            "Could not open archive to corrupt payload."
        );
    }

    // Read the original first payload byte.
    file.seekg(static_cast<std::streamoff>(payload_offset));

    char original_byte = 0;
    file.get(original_byte);

    if (!file) {
        throw std::runtime_error(
            "Could not read the first payload byte."
        );
    }

    // Flip one bit without changing the stored checksum.
    const char corrupted_byte =
        static_cast<char>(
            static_cast<unsigned char>(original_byte) ^ 0x01u
        );

    file.clear();
    file.seekp(static_cast<std::streamoff>(payload_offset));
    file.put(corrupted_byte);
    file.flush();

    if (!file) {
        throw std::runtime_error(
            "Failed to corrupt the first payload byte."
        );
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
    const fs::path checksum_archive = "checksum_corruption.uatc";

    try {
        const auto original_data =
            uatc::FileReader::read(source_file);

        const auto blocks =
            uatc::BlockSplitter::split(original_data, 4);

        // TEST 1: Raw archive round-trip.
        uatc::ArchiveWriter::write(
            raw_archive,
            source_file,
            blocks
        );

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
            source_file,
            invalid_codec_archive,
            99,
            4
        );

        if (!expect_failure(
                invalid_codec_archive,
                "Unsupported codec")) {
            throw std::runtime_error(
                "Unsupported codec was not rejected."
            );
        }

        std::cout << "TEST PASSED: Unsupported codec rejected.\n";

        // TEST 4: Invalid raw stored length.
        create_modified_archive(
            source_file,
            invalid_length_archive,
            static_cast<std::uint8_t>(uatc::CodecId::None),
            999
        );

        if (!expect_failure(
                invalid_length_archive,
                "stored length")) {
            throw std::runtime_error(
                "Invalid stored length was not rejected."
            );
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
            invalid_offset_archive,
            source_file,
            blocks
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

        // TEST 7: Corrupted payload must fail checksum verification.
        uatc::ArchiveWriter::write(
            checksum_archive,
            source_file,
            blocks
        );

        corrupt_first_payload_byte(checksum_archive);

        if (!expect_failure(
                checksum_archive,
                "Block checksum mismatch")) {
            throw std::runtime_error(
                "Corrupted payload was not rejected by checksum verification."
            );
        }

        std::cout << "TEST PASSED: Corrupted payload rejected by checksum.\n";

        remove_if_exists(raw_archive);
        remove_if_exists(invalid_codec_archive);
        remove_if_exists(invalid_length_archive);
        remove_if_exists(invalid_parameter_archive);
        remove_if_exists(invalid_offset_archive);
        remove_if_exists(checksum_archive);

        std::cout << "\nALL ARCHIVE READER TESTS PASSED!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "TEST FAILED: " << e.what() << '\n';

        remove_if_exists(raw_archive);
        remove_if_exists(invalid_codec_archive);
        remove_if_exists(invalid_length_archive);
        remove_if_exists(invalid_parameter_archive);
        remove_if_exists(invalid_offset_archive);
        remove_if_exists(checksum_archive);

        return 1;
    }
}
