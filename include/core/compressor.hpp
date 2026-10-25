#pragma once

// DAY 10 - Compression Manager.
//
// Orchestrates the whole per-block pipeline: split, probe, select the
// cheapest transform, encode, and write the archive (SAD 3.4). The CLI and
// the HTTP API layer both drive this class, so there is exactly one
// implementation of the pipeline.

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>

#include "archive/archive_format.hpp"
#include "core/config.hpp"
#include "core/progress.hpp"

namespace uatc::core {

struct CompressResult {
    std::uint64_t originalBytes = 0;
    std::uint64_t archiveBytes = 0;
    std::uint64_t blockCount = 0;
    std::uint64_t storedBlocks = 0;
    std::uint64_t compressedBlocks = 0;
    double ratio = 0.0;  // original / archive; below 1.0 means it grew
    std::map<TransformId, std::uint64_t> transformUsage;
};

struct ExtractResult {
    std::filesystem::path destination;
    std::string relativePath;
    std::uint64_t bytesWritten = 0;
};

class CompressionManager {
 public:
    // Compresses a file into a .uatc archive. Each block is probed and, if it
    // is worth compressing, the cheapest transform is selected for it.
    // If a sink is given, it is notified after every block.
    static CompressResult compress(
        const std::filesystem::path& source,
        const std::filesystem::path& archive,
        const CompressOptions& options = CompressOptions{},
        ProgressSink* progress = nullptr
    );

    // Restores the file held in an archive under the output directory, using
    // the relative path the archive recorded.
    static ExtractResult extract(
        const std::filesystem::path& archive,
        const std::filesystem::path& outputDirectory
    );
};

}  // namespace uatc::core
