#pragma once

// Progress reporting for the core facade (SAD 4.3.3, 4.4).
//
// The core reports progress through a sink so that the CLI, the HTTP API
// and the benchmark can each render it their own way without the core
// knowing anything about them.

#include <cstdint>

#include "archive/archive_format.hpp"

namespace uatc::core {

// Reported once per block, as the block is processed.
struct BlockProgress {
    std::uint64_t blockIndex = 0;   // 0-based index of the block
    std::uint64_t blockCount = 0;   // total number of blocks
    std::uint64_t bytesIn = 0;      // original bytes in this block
    std::uint64_t bytesOut = 0;     // bytes this block occupies in the archive
    TransformId transformId = TransformId::None;
    bool stored = false;            // true when the block was kept unchanged
};

class ProgressSink {
 public:
    virtual ~ProgressSink() = default;

    // Called after each block is processed.
    virtual void on_block(const BlockProgress& progress) = 0;
};

// A sink that counts blocks and bytes and reports nothing.
class NullProgressSink : public ProgressSink {
 public:
    void on_block(const BlockProgress& progress) override {
        ++blocksSeen;
        bytesIn += progress.bytesIn;
        bytesOut += progress.bytesOut;
    }

    std::uint64_t blocksSeen = 0;
    std::uint64_t bytesIn = 0;
    std::uint64_t bytesOut = 0;
};

}  // namespace uatc::core
