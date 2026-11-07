#include "core/planner.hpp"

#include <numeric>

#include "archive/block.hpp"
#include "archive/transforms.hpp"
#include "core/block_stats.hpp"
#include "core/baseline_entropy_coder.hpp"
#include "core/cost_estimator.hpp"
#include "core/huffman_coder.hpp"
#include "core/transform_factory.hpp"
#include "filesystem/file_reader.hpp"

namespace uatc::core {

BlockPlan plan_block(
    ByteSpan data,
    std::size_t index,
    std::uint64_t offset,
    const TransformSelector& selector,
    const CompressibilityProbe& probe
) {
    BlockPlan plan;
    plan.index = index;
    plan.offset = offset;
    plan.size = data.size;

    const auto stats = BlockStats::compute(data);
    plan.entropy = stats.entropy_estimate();
    plan.uniqueBytes = stats.unique_byte_count;

    plan.compressible = probe.is_compressible(data);

    // Incompressible by the probe: store, no work (FR-05 -> FR-08).
    if (!plan.compressible) {
        plan.transformId = TransformId::None;
        plan.codecId = CodecId::None;
        plan.estimatedCost = data.size;
        plan.stored = true;
        return plan;
    }

    // Compressible: the raw cost is the bar every candidate has to beat.
    const std::uint64_t raw_bar = data.size;

    // The selector picks the best transform for the block. Its own
    // store-versus-compress verdict is transform-only - it cannot see the
    // entropy coders - so it is consulted for the transform choice and the
    // final decision is made here, coder-aware.
    const auto selection = selector.select(data);
    plan.transformId = selection.transform_id;

    const std::vector<std::uint8_t> input(data.begin(), data.end());
    const auto transformed = uatc::Transform::encode(input, plan.transformId);

    // Price both coders on the transformed bytes; the smaller wins.
    static const BaselineEntropyCoder baseline;
    static const HuffmanCoder huffman;
    const auto baseline_cost = estimate_coded_size(
        transformed.data(), transformed.size());
    const auto huffman_result = huffman.encode(
        ByteSpan(transformed.data(), transformed.size()));

    std::size_t best_cost = baseline_cost;
    CodecId best_codec = CodecId::Baseline;
    if (huffman_result.data.size() < best_cost) {
        best_cost = huffman_result.data.size();
        best_codec = huffman.id();
    }

    // FR-08 with the real coder: the coded form plus its metadata must
    // beat the raw bytes, or the block is stored.
    if (best_cost + kTransformMetadataOverhead < raw_bar) {
        plan.codecId = best_codec;
        plan.estimatedCost = best_cost;
        plan.stored = false;
    } else {
        plan.transformId = TransformId::None;
        plan.codecId = CodecId::None;
        plan.estimatedCost = raw_bar;
        plan.stored = true;
    }
    return plan;
}

AnalysisReport analyze(
    const std::filesystem::path& source,
    const CompressOptions& options
) {
    if (!std::filesystem::exists(source)) {
        throw std::runtime_error("Input does not exist: " + source.string());
    }
    if (!std::filesystem::is_regular_file(source)) {
        throw std::runtime_error(
            "Input path is not a regular file: " + source.string()
        );
    }
    if (options.block_size == 0) {
        throw std::invalid_argument("Block size must be greater than zero.");
    }

    const auto data = FileReader::read(source);
    const auto blocks = BlockSplitter::split(data, options.block_size);

    const auto registry = make_default_registry();
    const TransformSelector selector(registry);
    const CompressibilityProbe probe;

    AnalysisReport report;
    report.totalBytes = data.size();
    report.blockCount = blocks.size();

    // Entropy over the whole file, not just the sampled blocks.
    const auto overall = BlockStats::compute(ByteSpan(data));
    report.overallEntropy = overall.entropy_estimate();

    report.blocks.reserve(blocks.size());
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const auto& block = blocks[i];
        auto plan = plan_block(ByteSpan(block.data), i, block.offset,
                               selector, probe);
        report.estimatedArchiveBytes += plan.estimatedCost;
        if (plan.stored) {
            ++report.storedBlocks;
        } else {
            ++report.compressibleBlocks;
        }
        report.blocks.push_back(plan);
    }

    if (report.estimatedArchiveBytes > 0) {
        report.estimatedRatio = static_cast<double>(report.totalBytes) /
                                static_cast<double>(report.estimatedArchiveBytes);
    }
    return report;
}

}  // namespace uatc::core
