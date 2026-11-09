#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "archive/archive_binary.hpp"
#include "archive/archive_format.hpp"
#include "archive/archive_reader.hpp"
#include "archive/archive_writer.hpp"
#include "archive/block.hpp"
#include "core/error.hpp"
#include "core/sha256.hpp"

namespace {
namespace fs = std::filesystem;

void expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void flip_byte(const fs::path& path, std::uint64_t offset) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file) {
        throw std::runtime_error("Could not open archive to corrupt.");
    }
    file.seekg(static_cast<std::streamoff>(offset));
    char value = 0;
    file.get(value);
    if (!file) {
        throw std::runtime_error("Could not read byte to corrupt.");
    }
    file.clear();
    file.seekp(static_cast<std::streamoff>(offset));
    file.put(static_cast<char>(static_cast<unsigned char>(value) ^ 1u));
    file.flush();
    if (!file) {
        throw std::runtime_error("Could not write corrupted archive byte.");
    }
}

std::uint64_t first_payload_offset(const fs::path& archive) {
    uatc::BinaryReader reader(archive);
    reader.read_u32();
    reader.read_u16();
    reader.read_u16();
    reader.read_u32();
    reader.read_u32();
    reader.read_u64();
    const std::uint64_t block_index_offset = reader.read_u64();
    reader.seekg(block_index_offset + 8);
    return reader.read_u64();
}

void expect_rejected(
    const fs::path& archive,
    const std::string& message,
    uatc::core::ErrorCode code
) {
    try {
        static_cast<void>(uatc::ArchiveReader::read(archive));
    } catch (const uatc::core::UatcError& error) {
        expect(error.code() == code, "Archive integrity error code mismatch.");
        expect(
            std::string(error.what()).find(message) != std::string::npos,
            "Archive integrity error message mismatch."
        );
        return;
    }
    throw std::runtime_error("Corrupted archive was accepted.");
}

}  // namespace

int main() {
    const auto unique = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    const auto root = fs::temp_directory_path() /
        ("uatc_integrity_test_" + std::to_string(unique));
    const auto source = root / "source.bin";
    const auto archive = root / "source.uatc";
    const std::vector<std::uint8_t> original = {'a', 'b', 'c'};

    try {
        fs::create_directories(root);
        {
            std::ofstream output(source, std::ios::binary);
            output.write(
                reinterpret_cast<const char*>(original.data()),
                static_cast<std::streamsize>(original.size())
            );
            if (!output) {
                throw std::runtime_error(
                    "Could not write archive integrity fixture."
                );
            }
        }

        const auto blocks = uatc::BlockSplitter::split(original, original.size());
        const auto write_archive = [&]() {
            uatc::ArchiveWriter::write(archive, source, blocks);
        };
        write_archive();

        const std::uint64_t archive_size = fs::file_size(archive);
        const std::uint64_t data_region_size =
            archive_size - uatc::UATC_INTEGRITY_REGION_SIZE;
        uatc::BinaryReader reader(archive);
        reader.seekg(data_region_size);
        const auto stored_file_sha = reader.read_bytes(
            static_cast<std::size_t>(uatc::UATC_SHA256_SIZE)
        );
        const auto expected_file_sha = uatc::core::sha256(
            original.data(),
            original.size()
        );
        expect(
            std::equal(
                expected_file_sha.begin(),
                expected_file_sha.end(),
                stored_file_sha.begin()
            ),
            "File SHA-256 is not stored in the integrity region."
        );

        reader.seekg(
            data_region_size + uatc::UATC_SHA256_SIZE
        );
        const auto stored_archive_sha = reader.read_bytes(
            static_cast<std::size_t>(uatc::UATC_SHA256_SIZE)
        );
        const auto expected_archive_sha =
            uatc::core::sha256_of_file_prefix(archive, data_region_size);
        expect(
            std::equal(
                expected_archive_sha.begin(),
                expected_archive_sha.end(),
                stored_archive_sha.begin()
            ),
            "Archive SHA-256 does not cover the archive data region."
        );

        reader.seekg(archive_size - uatc::UATC_ARCHIVE_FOOTER_SIZE);
        expect(reader.read_u64() == 0, "Footer header offset is invalid.");
        expect(
            reader.read_u32() == uatc::UATC_MAGIC,
            "Footer magic is invalid."
        );
        expect(
            uatc::ArchiveReader::read(archive) == original,
            "Archive with integrity footer did not round-trip."
        );

        write_archive();
        flip_byte(archive, first_payload_offset(archive));
        expect_rejected(
            archive,
            "Block checksum mismatch",
            uatc::core::ErrorCode::BlockChecksum
        );

        write_archive();
        flip_byte(archive, data_region_size);
        expect_rejected(
            archive,
            "File SHA-256 mismatch",
            uatc::core::ErrorCode::ArchiveChecksum
        );

        write_archive();
        flip_byte(
            archive,
            data_region_size + uatc::UATC_SHA256_SIZE
        );
        expect_rejected(
            archive,
            "Archive SHA-256 mismatch",
            uatc::core::ErrorCode::ArchiveChecksum
        );

        write_archive();
        flip_byte(
            archive,
            fs::file_size(archive) - 1
        );
        expect_rejected(
            archive,
            "integrity footer is invalid",
            uatc::core::ErrorCode::MalformedArchive
        );

        write_archive();
        flip_byte(
            archive,
            fs::file_size(archive) - uatc::UATC_ARCHIVE_FOOTER_SIZE
        );
        expect_rejected(
            archive,
            "integrity footer is invalid",
            uatc::core::ErrorCode::MalformedArchive
        );

        write_archive();
        fs::resize_file(archive, fs::file_size(archive) - 1);
        expect_rejected(
            archive,
            "integrity footer is invalid",
            uatc::core::ErrorCode::MalformedArchive
        );

        write_archive();
        fs::resize_file(
            archive,
            fs::file_size(archive) - uatc::UATC_INTEGRITY_REGION_SIZE
        );
        expect_rejected(
            archive,
            "integrity footer is invalid",
            uatc::core::ErrorCode::MalformedArchive
        );
    } catch (...) {
        std::error_code ignored;
        fs::remove_all(root, ignored);
        throw;
    }

    fs::remove_all(root);
}
