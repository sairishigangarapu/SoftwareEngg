// DAY 5 - Integration: full per-block compression pipeline.
// Demonstrates: split → stats → probe → NoneTransform → baseline entropy coder → round-trip.
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>
#include "core/block_slicer.hpp"
#include "core/block_stats.hpp"
#include "core/none_transform.hpp"
#include "core/baseline_entropy_coder.hpp"
#include "core/registry.hpp"
#include "archive/archive_format.hpp"

int main() {
    using namespace uatc::core;

    // --- Simulate a file with mixed content ---
    std::vector<std::uint8_t> file_data;
    // Random-ish prefix
    for (int i = 0; i < 500; ++i) file_data.push_back(static_cast<std::uint8_t>(i & 0xFF));
    // Long run of zeros
    for (int i = 0; i < 2000; ++i) file_data.push_back(0);
    // Mixed trailing bytes
    std::vector<std::uint8_t> tail = {1, 2, 3, 4, 4, 4, 4, 9, 8, 7, 7, 7, 7, 0, 0, 0};
    file_data.insert(file_data.end(), tail.begin(), tail.end());

    // --- Day 3: Block slicing ---
    constexpr std::size_t kBlockSize = 512;
    auto blocks = slice_blocks(ByteSpan(file_data), kBlockSize);
    assert(!blocks.empty());

    // Verify exact reconstruction
    auto rebuilt = reassemble(blocks);
    assert(rebuilt == file_data);

    // --- Day 2+3: BlockStats per block ---
    for (const auto& b : blocks) {
        auto stats = BlockStats::compute(ByteSpan(b.data));
        assert(stats.size == b.data.size());
        // entropy_estimate is in [0, 8] bits/byte
        double h = stats.entropy_estimate();
        assert(h >= 0.0 && h <= 8.0);
    }

    // --- Day 4: Transform (None) + Day 5: Entropy Coder ---
    NoneTransform none;
    BaselineEntropyCoder coder;
    TransformRegistry registry;
    registry.register_transform(std::make_unique<NoneTransform>());

    // Process each block through the pipeline
    ByteBuffer reconstructed;
    reconstructed.reserve(file_data.size());
    std::size_t total_encoded = 0;
    std::size_t total_decoded = 0;

    for (const auto& block : blocks) {
        // Stage 1: Transform (None = passthrough)
        auto t_result = none.encode(ByteSpan(block.data));
        assert(t_result.data.size() == block.data.size());

        // Stage 2: Entropy code the transformed data
        auto c_result = coder.encode(ByteSpan(t_result.data));
        total_encoded += c_result.data.size();

        // Stage 3: Decode (reverse entropy + reverse transform)
        auto decoded_bytes = coder.decode(ByteSpan(c_result.data), c_result.original_size);
        total_decoded += decoded_bytes.size();

        // Stage 4: Decode transform (None = identity)
        auto final_bytes = none.decode(ByteSpan(decoded_bytes), 0);
        assert(final_bytes == block.data);
        reconstructed.insert(reconstructed.end(), final_bytes.begin(), final_bytes.end());
    }

    // Verify full round-trip
    assert(reconstructed == file_data);

    // --- Day 5: Measure encoded size ---
    std::printf("pipeline_test: PASS\n");
    std::printf("  original:   %zu bytes\n", file_data.size());
    std::printf("  encoded:    %zu bytes\n", total_encoded);
    std::printf("  ratio:      %.3f\n",
                static_cast<double>(total_encoded) / static_cast<double>(total_decoded));
    std::printf("  blocks:     %zu\n", blocks.size());

    // Verify the zero-run block compressed well
    {
        auto blocks2 = slice_blocks(ByteSpan(file_data), kBlockSize);
        auto& zero_block = blocks2[3];  // block with zeros (512+2000+16 > 512)
        auto stats = BlockStats::compute(ByteSpan(zero_block.data));
        // If this block has many zeros, entropy should be low
        if (stats.unique_byte_count < 50) {
            auto encoded = coder.encode(ByteSpan(zero_block.data));
            assert(encoded.data.size() < zero_block.data.size());
        }
    }

    return 0;
}
