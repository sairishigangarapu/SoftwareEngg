#include "core/planner.hpp"

#include <numeric>

#include "archive/block.hpp"
#include "core/block_stats.hpp"
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

    if (!plan.compressible) {
        // Incompressible: keep the original bytes (FR-08).
        plan.transformId = TransformId::None;
        plan.codecId = CodecId::None;
        plan.estimatedCost = data.size;
        plan.stored = true;
        return plan;
    }

    const auto selection = selector.select(data);
    plan.transformId = selection.transform_id;
    plan.codecId = CodecId::Baseline;
    plan.estimatedCost = selection.estimated_cost;
    plan.stored = selection.stored_raw;
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
