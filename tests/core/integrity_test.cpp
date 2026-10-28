// DAY 12 - Archive integrity region tests (SAD 4.1, TC-INT-01, TC-INT-02).
//
// The integrity region is fixed size (32 file SHA + 32 archive SHA + 12
// footer), appended after the payloads. These tests check what each failure
// is reported as: a byte flipped in a payload is a block failure with a
// block index; a byte flipped in the stored archive checksum is an
// archive-level failure.
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

#include "archive/archive_writer.hpp"
#include "archive/archive_reader.hpp"
#include "core/compressor.hpp"
#include "core/error.hpp"

namespace fs = std::filesystem;

namespace {

void write_file(const fs::path& p, const std::vector<std::uint8_t>& data) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
}

std::vector<std::uint8_t> read_back(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<std::uint8_t>(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());
}

void flip_byte_at(const fs::path& p, std::uint64_t offset) {
    std::fstream f(p, std::ios::binary | std::ios::in | std::ios::out);
    if (!f) throw std::runtime_error("cannot open " + p.string());
    f.seekg(static_cast<std::streamoff>(offset));
    char byte = 0;
    f.read(&byte, 1);
    byte = static_cast<char>(static_cast<unsigned char>(byte) ^ 0xFFu);
    f.seekp(static_cast<std::streamoff>(offset));
    f.write(&byte, 1);
}

// A copy that cannot silently fail the way copy_file can.
void copy_file_bytes(const fs::path& from, const fs::path& to) {
    std::ifstream in(from, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + from.string());
    std::ofstream out(to, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write " + to.string());
    out << in.rdbuf();
    if (!out) throw std::runtime_error("copy failed " + to.string());
}

}  // namespace

int main() {
    using namespace uatc;
    using namespace uatc::core;

    const fs::path work = "uatc_integrity_test_dir";
    fs::remove_all(work);
    fs::create_directories(work);

    // Mixed content, several blocks, so the block index is not trivial.
    std::vector<std::uint8_t> data;
    for (int i = 0; i < 50000; ++i)
        data.push_back(static_cast<std::uint8_t>((i / 4) & 0xFF));
    for (int i = 0; i < 50000; ++i) data.push_back(0x00);
    {
        std::mt19937 rng(5);
        for (int i = 0; i < 30000; ++i)
            data.push_back(static_cast<std::uint8_t>(rng()));
    }

    const auto source = work / "integrity.bin";
    write_file(source, data);

    CompressOptions options;
    options.block_size = 32 * 1024;

    // --- A valid archive verifies and round-trips ---
    const auto archive = work / "integrity.uatc";
    CompressionManager::compress(source, archive, options);
    {
        auto restored = CompressionManager::extract(archive, work / "ok");
        assert(read_back(restored.destination) == data);
    }

    const auto size = std::filesystem::file_size(archive);
    // Integrity region = 32 (file SHA) + 32 (archive SHA) + 12 (footer).
    const std::uint64_t integrity_region = 76;
    assert(size > integrity_region);
    const std::uint64_t payload_start = size - integrity_region;

    // --- TC-INT-01: flip a byte inside a block payload -> a block-level
    //     failure, not an archive-level one. The last payload byte sits just
    //     before the integrity region: size - integrity_region - 1. ---
    {
        const auto flipped = work / "flipped_payload.uatc";
        copy_file_bytes(archive, flipped);
        flip_byte_at(flipped, payload_start - 1);

        bool caught = false;
        std::string message;
        try {
            ArchiveReader::read(flipped);
        } catch (const std::exception& error) {
            caught = true;
            message = error.what();
        }
        assert(caught);
        // The block checksum is checked before the archive and file
        // checksums, so the diagnosis names the block.
        assert(message == "Block checksum mismatch.");
    }

    // --- TC-INT-02: flip a byte inside the stored archive checksum ->
    //     E_ARCHIVE_CHECKSUM, with everything else intact. The archive
    //     SHA-256 starts 32 bytes of file SHA + 32 bytes from the end. ---
    {
        const auto corrupted = work / "flipped_archive_sha.uatc";
        copy_file_bytes(archive, corrupted);
        flip_byte_at(corrupted, size - 44);

        bool caught = false;
        std::string message;
        try {
            ArchiveReader::read(corrupted);
        } catch (const std::exception& error) {
            caught = true;
            message = error.what();
        }
        assert(caught);
        assert(message == "Archive checksum mismatch.");
    }

    // --- Truncation removes the footer and is rejected ---
    {
        const auto truncated = work / "truncated.uatc";
        copy_file_bytes(archive, truncated);
        std::filesystem::resize_file(truncated, size - 1);

        bool caught = false;
        try {
            ArchiveReader::read(truncated);
        } catch (const std::exception&) {
            caught = true;
        }
        assert(caught);
    }

    std::filesystem::remove_all(work);
    std::printf("integrity_test: PASS\n");
    return 0;
}
