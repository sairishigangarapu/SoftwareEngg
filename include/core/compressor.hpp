#pragma once

#include <cstddef>
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
    double ratio = 0.0;
    std::map<TransformId, std::uint64_t> transformUsage;
};

struct ExtractResult {
    std::filesystem::path destination;
    std::string relativePath;
    std::uint64_t bytesWritten = 0;
};

class CompressionManager {
public:
    static CompressResult compress(
        const std::filesystem::path& source,
        const std::filesystem::path& archive,
        const CompressOptions& options = CompressOptions{},
        ProgressSink* progress = nullptr
    );

    // worker_count controls concurrent block decoding. Defaults to 1 for
    // compatibility with existing callers.
    static ExtractResult extract(
        const std::filesystem::path& archive,
        const std::filesystem::path& outputDirectory,
        std::size_t worker_count = 1
    );
};

} // namespace uatc::core