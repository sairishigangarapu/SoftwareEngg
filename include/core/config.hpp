#pragma once

// DAY 2 - Configurable block size for the MVP.
#include <cstddef>
#include <cstdint>

namespace uatc::core {

// Default block size: 1 MiB.
// Chosen as a balance between parallelism granularity and memory overhead.
// Configurable via Config.block_size at runtime; never hard-coded in call sites.
constexpr std::size_t kDefaultBlockSize = 1 << 20;

struct Config {
    std::size_t block_size = kDefaultBlockSize;
};

// Options for a compression or analysis run.
struct CompressOptions {
    std::size_t block_size = kDefaultBlockSize;

    // Worker threads for the block pipeline. 1 means the serial path.
    // Results are identical for any worker count (FR-10, ADR-05), so this
    // only affects speed, never the archive.
    std::size_t workers = 1;
};

}  // namespace uatc::core
