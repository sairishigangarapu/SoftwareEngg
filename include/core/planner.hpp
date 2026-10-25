#pragma once

// Planning and analysis: what the engine is going to do to a block, and to
// a whole file, before it does any of it.
//
// The same planner drives both the "before" report and the real compression,
// so the prediction and the outcome can never disagree.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

#include "archive/archive_format.hpp"
#include "core/byte_buffer.hpp"
#include "core/compressibility_probe.hpp"
#include "core/config.hpp"
#include "core/transform_selector.hpp"

namespace uatc::core {

// What the engine intends to do with one block.
struct BlockPlan {
    std::size_t index = 0;
    std::uint64_t offset = 0;
    std::size_t size = 0;

    double entropy = 0.0;          // Shannon entropy, bits per byte
    std::size_t uniqueBytes = 0;   // distinct byte values present
    bool compressible = false;     // the probe's verdict

    TransformId transformId = TransformId::None;
    CodecId codecId = CodecId::None;
    std::size_t estimatedCost = 0; // bytes the block is expected to occupy
    bool stored = false;           // true when the block is kept unchanged
};

// The whole-file prediction.
struct AnalysisReport {
    std::uint64_t totalBytes = 0;
    std::size_t blockCount = 0;
    double overallEntropy = 0.0;
    std::size_t compressibleBlocks = 0;
    std::size_t storedBlocks = 0;
    std::size_t estimatedArchiveBytes = 0;  // predicted archive size
    double estimatedRatio = 0.0;             // original / predicted archive
    std::vector<BlockPlan> blocks;
};

// Decides what to do with one block. Pure: the same block always yields the
// same plan.
BlockPlan plan_block(
    ByteSpan data,
    std::size_t index,
    std::uint64_t offset,
    const TransformSelector& selector,
    const CompressibilityProbe& probe
);

// Analyses a file without writing anything.
AnalysisReport analyze(
    const std::filesystem::path& source,
    const CompressOptions& options = CompressOptions{}
);

}  // namespace uatc::core
