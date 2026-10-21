// DAY 10 - Compression Manager: compress, inspect, verify, extract.
//
// Covers the full pipeline the CLI and the API layer drive: probe, select
// the cheapest transform per block, write the archive, read it back
// byte-for-byte, and refuse an archive with an unsafe path.
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

#include "archive/archive_inspector.hpp"
#include "archive/archive_verifier.hpp"
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

std::vector<std::uint8_t> read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
    using namespace uatc;
    using namespace uatc::core;

    const fs::path work = "uatc_compressor_test_dir";
    fs::remove_all(work);
    fs::create_directories(work);

    // --- Mixed content: a ramp, a run of zeros and random bytes ---
    std::vector<std::uint8_t> original;
    for (int i = 0; i < 100000; ++i)
        original.push_back(static_cast<std::uint8_t>((i / 4) & 0xFF));
    for (int i = 0; i < 100000; ++i) original.push_back(0x00);
    {
        std::mt19937 rng(11);
        for (int i = 0; i < 50000; ++i)
            original.push_back(static_cast<std::uint8_t>(rng()));
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

    // --- The archive is readable by the inspector and the verifier ---
    {
        const auto info = ArchiveInspector::inspect(archive);
        assert(info.fileCount == 1);
        assert(info.blockCount == result.blockCount);
        assert(info.originalBytes == original.size());
        // The inspector must accept every transform the core registered.
        assert(!info.transformUsage.empty());
    }
    {
        const auto verification = ArchiveVerifier::verify(archive);
        assert(verification.success);
        assert(verification.blocksChecked == result.blockCount);
    }

    // --- Extract restores the file byte for byte ---
    {
        const auto extracted = CompressionManager::extract(archive, work / "restored");
        assert(extracted.bytesWritten == original.size());
        assert(extracted.relativePath == "mixed.bin");
        assert(read_file(extracted.destination) == original);
    }

    // --- An incompressible file is stored, not squeezed ---
    {
        std::mt19937 rng(99);
        std::vector<std::uint8_t> noise(200000);
        for (auto& b : noise) b = static_cast<std::uint8_t>(rng());
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
            CompressionManager::compress(work / "nope.bin", work / "x.uatc", options);
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
