
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "archive/archive_verifier.hpp"
#include "archive/archive_writer.hpp"
#include "archive/block.hpp"
#include "filesystem/file_reader.hpp"

int main() {
    namespace fs = std::filesystem;

    const fs::path source = "test_data/hello.txt";
    const fs::path archive = "verifier_test.uatc";

    try {
        const auto original = uatc::FileReader::read(source);
        const auto blocks = uatc::BlockSplitter::split(original, 4);

        uatc::ArchiveWriter::write(archive, source, blocks);

        // Test 1: A valid archive should verify successfully.
        const auto valid = uatc::ArchiveVerifier::verify(archive);

        if (!valid.success) {
            throw std::runtime_error(
                "Valid archive failed verification: " + valid.message
            );
        }

        if (valid.filesChecked != 1 ||
            valid.blocksChecked != blocks.size()) {
            throw std::runtime_error(
                "Incorrect verification counts for valid archive."
            );
        }

        std::cout << "PASS: Valid archive verified.\n";

        // Test 2: Corrupt a payload byte and expect verification to fail.
        {
            std::fstream file(
                archive,
                std::ios::binary | std::ios::in | std::ios::out
            );

            if (!file) {
                throw std::runtime_error(
                    "Could not open archive for corruption test."
                );
            }

            // The writer places the payload after the header, file record,
            // filename, and block index. Find the payload offset from the
            // first block record instead of assuming a fixed filename size.
            file.seekg(24, std::ios::beg);

            auto read_u64 = [&file]() -> std::uint64_t {
                std::uint64_t value = 0;

                for (int i = 0; i < 8; ++i) {
                    char byte = 0;
                    if (!file.get(byte)) {
                        throw std::runtime_error(
                            "Could not read archive metadata."
                        );
                    }

                    value |=
                        static_cast<std::uint64_t>(
                            static_cast<unsigned char>(byte)
                        ) << (i * 8);
                }

                return value;
            };

            // Header offsets: file-table offset at byte 16,
            // block-index offset at byte 24.
            const std::uint64_t block_index_offset = read_u64();

            file.clear();
            file.seekg(
                static_cast<std::streamoff>(block_index_offset + 8),
                std::ios::beg
            );

            const std::uint64_t payload_offset = read_u64();

            file.clear();
            file.seekg(
                static_cast<std::streamoff>(payload_offset),
                std::ios::beg
            );

            char byte = 0;
            if (!file.get(byte)) {
                throw std::runtime_error(
                    "Could not read payload for corruption test."
                );
            }

            file.clear();
            file.seekp(
                static_cast<std::streamoff>(payload_offset),
                std::ios::beg
            );
            file.put(static_cast<char>(
                static_cast<unsigned char>(byte) ^ 0x01u
            ));
            file.flush();

            if (!file) {
                throw std::runtime_error(
                    "Could not corrupt archive payload."
                );
            }
        }

        const auto corrupted = uatc::ArchiveVerifier::verify(archive);

        if (corrupted.success) {
            throw std::runtime_error(
                "Corrupted archive incorrectly passed verification."
            );
        }

        std::cout << "PASS: Corrupted archive rejected.\n";

        fs::remove(archive);
        std::cout << "All archive verifier tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code ignored;
        fs::remove(archive, ignored);

        std::cerr << "Archive verifier test failed: "
                  << error.what() << '\n';
        return 1;
    }
}
