#pragma once

// Core facade for the CLI and the API layer (SAD 4.3.3).
//
// One entry point per operation, for a file or a whole folder, so the CLI
// and the HTTP API never carry their own copy of the pipeline:
//
//   compress_path  - a regular file -> single-file archive
//                    a folder       -> one multi-file archive (FR-01/02)
//   extract_path   - any .uatc      -> the original file or folder (FR-03)
//
// Both report progress through the same ProgressSink, and both fail with
// UatcError carrying a SAD 4.3.4 code.

#include <cstdint>
#include <filesystem>
#include <vector>

#include "core/compressor.hpp"
#include "core/progress.hpp"

namespace uatc::core {

struct ExtractSummary {
    std::filesystem::path destination;   // the folder the files went into
    std::uint64_t files = 0;
    std::uint64_t bytesWritten = 0;
    std::vector<std::filesystem::path> written;   // every file created
};

// Compresses a file or a folder into one archive. For a folder, every
// regular file below it is stored under its relative path, in sorted order
// (deterministic for any worker count, FR-10). The archive itself is
// skipped if it lies inside the folder being compressed.
//
// Throws UatcError: E_NOT_FOUND (no source), E_INVALID_ARGUMENT (block
// size 0, or a folder with no regular files).
CompressResult compress_path(const std::filesystem::path& source,
                             const std::filesystem::path& archive,
                             const CompressOptions& options = CompressOptions{},
                             ProgressSink* progress = nullptr);

// Restores an archive under destination_dir. A folder archive is fully
// read and verified (every CRC32 and the SHA-256 integrity data) before
// the first byte is written; if writing then fails part-way, the files
// already created are removed again (SAD 4.4). Every recorded path goes
// through safe_relative_path (TC-SEC-01).
ExtractSummary extract_path(const std::filesystem::path& archive,
                            const std::filesystem::path& destination_dir);

}  // namespace uatc::core
