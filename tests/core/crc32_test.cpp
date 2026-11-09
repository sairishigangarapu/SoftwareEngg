#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "archive/archive_binary.hpp"
#include "archive/archive_reader.hpp"
#include "archive/archive_writer.hpp"
#include "archive/block.hpp"
#include "core/crc32.hpp"

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::uint32_t crc_of(const std::string& value) {
    return uatc::core::crc32(
        reinterpret_cast<const std::uint8_t*>(value.data()),
        value.size()
    );
}

void check_known_vectors() {
    expect(crc_of("") == 0x00000000u, "Empty CRC32 vector failed.");
    expect(crc_of("a") == 0xE8B7BE43u, "Single-byte CRC32 vector failed.");
    expect(crc_of("abc") == 0x352441C2u, "CRC32 abc vector failed.");
    expect(
        crc_of("123456789") == 0xCBF43926u,
        "CRC32 standard check vector failed."
    );
    expect(
        crc_of("The quick brown fox jumps over the lazy dog") ==
            0x414FA339u,
        "CRC32 sentence vector failed."
    );

    const std::string value = "123456789";
    std::uint32_t crc = uatc::core::kCrc32Initial;
    crc = uatc::core::crc32_update(
        crc,
        reinterpret_cast<const std::uint8_t*>(value.data()),
        4
    );
    crc = uatc::core::crc32_update(
        crc,
        reinterpret_cast<const std::uint8_t*>(value.data()) + 4,
        5
    );
    expect(
        uatc::core::crc32_finish(crc) == 0xCBF43926u,
        "Incremental CRC32 update failed."
    );
}

void check_archive_integration() {
    namespace fs = std::filesystem;

    const auto unique = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    const auto root = fs::temp_directory_path() /
        ("uatc_crc32_test_" + std::to_string(unique));
    const auto source = root / "source.bin";
    const auto archive = root / "source.uatc";
    const std::vector<std::uint8_t> original = {
        '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };

    try {
        fs::create_directories(root);
        {
            std::ofstream output(source, std::ios::binary);
            output.write(
                reinterpret_cast<const char*>(original.data()),
                static_cast<std::streamsize>(original.size())
            );
            if (!output) {
                throw std::runtime_error("Could not write CRC32 fixture.");
            }
        }

        const auto blocks = uatc::BlockSplitter::split(
            original,
            original.size()
        );
        uatc::ArchiveWriter::write(archive, source, blocks);

        uatc::BinaryReader reader(archive);
        reader.read_u32();
        reader.read_u16();
        reader.read_u16();
        reader.read_u32();
        reader.read_u32();
        reader.read_u64();
        const auto block_index_offset = reader.read_u64();
        reader.seekg(block_index_offset + 36);

        expect(
            reader.read_u32() == 0xCBF43926u,
            "Archive block index did not store CRC32."
        );
        expect(
            uatc::ArchiveReader::read(archive) == original,
            "CRC32 archive round-trip failed."
        );
    } catch (...) {
        std::error_code ignored;
        fs::remove_all(root, ignored);
        throw;
    }

    fs::remove_all(root);
}

}  // namespace

int main() {
    check_known_vectors();
    check_archive_integration();
}
