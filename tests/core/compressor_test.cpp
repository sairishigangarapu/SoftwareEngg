
 // DAY 10 - Compression Manager: compress, inspect, verify, extract.
 //
 // Covers compression, archive inspection and verification, byte-for-byte
 // extraction, corruption handling, incompressible data, and error codes.

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "archive/archive_inspector.hpp"
#include "archive/archive_verifier.hpp"
#include "core/compressor.hpp"
#include "core/error.hpp"
#include "core/sha256.hpp"

namespace fs = std::filesystem;

namespace {

void write_file(const fs::path& p, const std::vector<std::uint8_t>& data) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    assert(out);
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
    assert(out);
}

std::vector<std::uint8_t> read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>());
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void replace_path_and_rehash(
    const fs::path& archive,
    const std::string& original_path,
    const std::string& replacement_path
) {
    require(original_path.size() == replacement_path.size(),
            "replacement archive path has a different size");
    auto bytes = read_file(archive);
    const std::vector<std::uint8_t> original(
        original_path.begin(), original_path.end());
    const auto found = std::search(bytes.begin(), bytes.end(),
                                   original.begin(), original.end());
    require(found != bytes.end(), "archive path was not found");
    std::copy(replacement_path.begin(), replacement_path.end(), found);

    constexpr std::size_t integrity_region_size = 32 + 32 + 12;
    require(bytes.size() >= integrity_region_size,
            "archive is too short to contain its integrity footer");
    const auto integrity_offset = bytes.size() - integrity_region_size;
    const auto archive_digest_offset = integrity_offset + 32;
    const auto digest = uatc::core::sha256(bytes.data(), integrity_offset);
    std::copy(digest.begin(), digest.end(),
              bytes.begin() +
                  static_cast<std::ptrdiff_t>(archive_digest_offset));

    std::ofstream out(archive, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(out), "could not rewrite test archive");
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(out), "could not finish rewriting test archive");
}

// Corrupt a byte in the archive payload using the block-index offset.
// The archive header layout is:
// magic (u32), version (u16), header size (u16), file count (u32),
// block count (u32), file-table offset (u64), block-index offset (u64).
bool corrupt_first_payload_byte(const fs::path& archive) {
    std::fstream file(archive, std::ios::binary | std::ios::in | std::ios::out);
    if (!file) {
        return false;
    }

    auto read_u16 = [&file](std::uint16_t& value) {
        unsigned char bytes[2]{};
        file.read(reinterpret_cast<char*>(bytes), 2);
        if (!file) return false;
        value = static_cast<std::uint16_t>(bytes[0]) |
                (static_cast<std::uint16_t>(bytes[1]) << 8);
        return true;
    };

    auto read_u32 = [&file](std::uint32_t& value) {
        unsigned char bytes[4]{};
        file.read(reinterpret_cast<char*>(bytes), 4);
        if (!file) return false;
        value = 0;
        for (unsigned i = 0; i < 4; ++i) {
            value |= static_cast<std::uint32_t>(bytes[i]) << (8 * i);
        }
        return true;
    };

    auto read_u64 = [&file](std::uint64_t& value) {
        unsigned char bytes[8]{};
        file.read(reinterpret_cast<char*>(bytes), 8);
        if (!file) return false;
        value = 0;
        for (unsigned i = 0; i < 8; ++i) {
            value |= static_cast<std::uint64_t>(bytes[i]) << (8 * i);
        }
        return true;
    };

    std::uint32_t ignored_u32 = 0;
    std::uint16_t ignored_u16 = 0;
    std::uint64_t ignored_u64 = 0;
    std::uint64_t block_index_offset = 0;

    // Skip magic, version, header size, file count, and block count.
    if (!read_u32(ignored_u32) ||
        !read_u16(ignored_u16) ||
        !read_u16(ignored_u16) ||
        !read_u32(ignored_u32) ||
        !read_u32(ignored_u32) ||
        !read_u64(ignored_u64) ||
        !read_u64(block_index_offset)) {
        return false;
    }

    // Each block-index entry begins with file ID and block ID,
    // followed by the payload offset.
    file.clear();
    file.seekg(static_cast<std::streamoff>(block_index_offset));
    if (!file) return false;

    if (!read_u32(ignored_u32) ||  // file ID
        !read_u32(ignored_u32)) {   // block ID
        return false;
    }

    std::uint64_t payload_offset = 0;
    if (!read_u64(payload_offset)) {
        return false;
    }

    file.clear();
    file.seekg(static_cast<std::streamoff>(payload_offset));
    char byte = 0;
    file.get(byte);
    if (!file) return false;

    file.clear();
    file.seekp(static_cast<std::streamoff>(payload_offset));
    file.put(static_cast<char>(
        static_cast<unsigned char>(byte) ^ 0x01u));
    file.flush();

    return static_cast<bool>(file);
}

}  // namespace

int main() {
    using namespace uatc;
    using namespace uatc::core;

    const fs::path work = "uatc_compressor_test_dir";
    fs::remove_all(work);
    fs::create_directories(work);

    // --- Mixed content: a ramp, a run of zeros, and random bytes ---
    std::vector<std::uint8_t> original;
    for (int i = 0; i < 100000; ++i) {
        original.push_back(static_cast<std::uint8_t>((i / 4) & 0xFF));
    }
    for (int i = 0; i < 100000; ++i) {
        original.push_back(0x00);
    }
    {
        std::mt19937 rng(11);
        for (int i = 0; i < 50000; ++i) {
            original.push_back(static_cast<std::uint8_t>(rng()));
        }
    }

    const auto source = work / "mixed.bin";
    const auto archive = work / "mixed.uatc";
    write_file(source, original);

    // --- Compress ---
    CompressOptions options;
    options.block_size = 64 * 1024;

    const auto result = CompressionManager::compress(source, archive, options);

    assert(result.originalBytes == original.size());
    assert(result.blockCount > 0);
    assert(result.storedBlocks + result.compressedBlocks == result.blockCount);
    assert(result.archiveBytes > 0);
    assert(!result.transformUsage.empty());

    // --- The archive is readable by the inspector and verifier ---
    {
        const auto info = ArchiveInspector::inspect(archive);
        assert(info.fileCount == 1);
        assert(info.blockCount == result.blockCount);
        assert(info.originalBytes == original.size());
        assert(!info.transformUsage.empty());
    }
    {
        const auto verification = ArchiveVerifier::verify(archive);
        assert(verification.success);
        assert(verification.blocksChecked == result.blockCount);
    }

    // --- Extract restores the file byte for byte ---
    {
        const auto extracted =
            CompressionManager::extract(archive, work / "restored");

        assert(extracted.bytesWritten == original.size());
        assert(extracted.relativePath == "mixed.bin");
        assert(read_file(extracted.destination) == original);
    }

    // --- Unsafe paths and symlink targets are rejected before extraction ---
    {
        const auto safe_source = work / "safe.txt";
        const auto safe_archive = work / "safe.uatc";
        write_file(safe_source, {1, 2, 3});
        CompressionManager::compress(safe_source, safe_archive, options);

        const std::vector<std::string> unsafe_paths = {
            "../x.txt",
            "C:/x.txt"
        };
        for (std::size_t i = 0; i < unsafe_paths.size(); ++i) {
            const auto unsafe_archive =
                work / ("unsafe-path-" + std::to_string(i) + ".uatc");
            fs::copy_file(safe_archive, unsafe_archive);
            replace_path_and_rehash(
                unsafe_archive, "safe.txt", unsafe_paths[i]);

            const auto output = work / ("unsafe-output-" + std::to_string(i));
            bool rejected = false;
            try {
                CompressionManager::extract(unsafe_archive, output);
            } catch (const UatcError& error) {
                rejected = true;
                if (error.code() != ErrorCode::PathTraversal) {
                    throw std::runtime_error(
                        std::string("unsafe archive path returned ") +
                        error_code_name(error.code()) + ": " + error.what()
                    );
                }
            }
            require(rejected, "unsafe archive path was accepted");
            require(!fs::exists(output),
                    "unsafe archive path created partial output");
        }
        require(!fs::exists(work / "x.txt"),
                "traversal archive wrote outside the extraction root");

        const auto dotted_archive = work / "dotted-name.uatc";
        fs::copy_file(safe_archive, dotted_archive);
        replace_path_and_rehash(dotted_archive, "safe.txt", "a..b.txt");
        const auto dotted_output =
            CompressionManager::extract(dotted_archive, work / "dotted-output");
        require(dotted_output.destination.filename() == "a..b.txt",
                "valid filename containing adjacent dots was rejected");
        require(read_file(dotted_output.destination) ==
                    std::vector<std::uint8_t>{1, 2, 3},
                "valid dotted filename extraction changed file contents");

        const auto outside = work / "outside.txt";
        write_file(outside, {9, 8, 7});
        const auto symlink_root = work / "symlink-output";
        fs::create_directories(symlink_root);
        std::error_code symlink_error;
        fs::create_symlink(outside, symlink_root / "safe.txt", symlink_error);
        if (!symlink_error) {
            bool rejected = false;
            try {
                CompressionManager::extract(safe_archive, symlink_root);
            } catch (const UatcError& error) {
                rejected = true;
                require(error.code() == ErrorCode::PathTraversal,
                        "symlink extraction returned the wrong error code");
            }
            require(rejected, "extraction followed a symlink target");
            require(read_file(outside) ==
                        std::vector<std::uint8_t>{9, 8, 7},
                    "symlink extraction modified the target");
        } else {
            std::printf(
                "SKIPPED symlink extraction check: %s\n",
                symlink_error.message().c_str()
            );
        }
    }

    // --- A corrupted archive is rejected without writing the output file ---
    {
        const fs::path corrupt_archive = work / "corrupt.uatc";
        const fs::path corrupt_output = work / "corrupt_output";

        fs::copy_file(archive, corrupt_archive,
                      fs::copy_options::overwrite_existing);

        assert(corrupt_first_payload_byte(corrupt_archive));

        bool caught = false;
        try {
            CompressionManager::extract(corrupt_archive, corrupt_output);
        } catch (const std::exception&) {
            caught = true;
        }

        assert(caught);
        assert(!fs::exists(corrupt_output / "mixed.bin"));
    }

    // --- An incompressible file is stored, not squeezed ---
    {
        std::mt19937 rng(99);
        std::vector<std::uint8_t> noise(200000);
        for (auto& b : noise) {
            b = static_cast<std::uint8_t>(rng());
        }

        const auto noise_source = work / "noise.bin";
        const auto noise_archive = work / "noise.uatc";
        write_file(noise_source, noise);

        const auto noise_result =
            CompressionManager::compress(noise_source, noise_archive, options);

        assert(noise_result.storedBlocks == noise_result.blockCount);
        assert(noise_result.compressedBlocks == 0);
        assert(noise_result.ratio <= 1.0);

        const auto restored =
            CompressionManager::extract(noise_archive, work / "restored_noise");
        assert(read_file(restored.destination) == noise);
    }

    // --- Compressible data is actually compressed ---
    {
        std::vector<std::uint8_t> zeros(500000, 0);
        const auto zero_source = work / "zeros.bin";
        const auto zero_archive = work / "zeros.uatc";
        write_file(zero_source, zeros);

        const auto zero_result =
            CompressionManager::compress(zero_source, zero_archive, options);

        assert(zero_result.compressedBlocks > 0);
        assert(zero_result.ratio > 1.5);
    }

    // --- A missing input is reported as E_NOT_FOUND ---
    {
        bool caught = false;
        try {
            CompressionManager::compress(work / "nope.bin",
                                         work / "x.uatc", options);
        } catch (const UatcError& error) {
            caught = true;
            assert(error.code() == ErrorCode::NotFound);
            assert(error_exit_code(error.code()) == 3);
        }
        assert(caught);
    }

    // --- A zero block size is rejected ---
    {
        bool caught = false;
        try {
            CompressOptions bad;
            bad.block_size = 0;
            CompressionManager::compress(source, work / "x.uatc", bad);
        } catch (const UatcError& error) {
            caught = true;
            assert(error.code() == ErrorCode::InvalidArgument);
        }
        assert(caught);
    }

    // --- Error codes carry the CLI exit codes from SAD 4.3.4 ---
    assert(error_exit_code(ErrorCode::InvalidArgument) == 2);
    assert(error_exit_code(ErrorCode::NotFound) == 3);
    assert(error_exit_code(ErrorCode::Permission) == 4);
    assert(error_exit_code(ErrorCode::Io) == 5);
    assert(error_exit_code(ErrorCode::MalformedArchive) == 10);
    assert(error_exit_code(ErrorCode::UnsupportedVersion) == 11);
    assert(error_exit_code(ErrorCode::MissingBlock) == 12);
    assert(error_exit_code(ErrorCode::BlockChecksum) == 13);
    assert(error_exit_code(ErrorCode::ArchiveChecksum) == 14);
    assert(error_exit_code(ErrorCode::PathTraversal) == 15);
    assert(std::string(error_code_name(ErrorCode::BlockChecksum)) ==
           "E_BLOCK_CHECKSUM");

    fs::remove_all(work);
    std::printf("compressor_test: PASS\n");
    return 0;
}
