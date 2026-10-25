// DAY 3 - Edge case tests for block slicing and reconstruction.
#include <cassert>
#include <cstdio>
#include <vector>
#include "core/block_slicer.hpp"
#include "core/block_stats.hpp"

int main() {
    using namespace uatc::core;

    // 1. Empty input → zero blocks
    {
        auto blocks = slice_blocks(ByteSpan{}, 1024);
        assert(blocks.empty());
    }

    // 2. Input smaller than block_size → one block with all data
    {
        std::vector<std::uint8_t> data = {1, 2, 3};
        auto blocks = slice_blocks(ByteSpan(data), 1024);
        assert(blocks.size() == 1);
        assert(blocks[0].data == data);
        assert(blocks[0].offset == 0);
        assert(blocks[0].block_id == 0);
    }

    // 3. Input exactly equal to block_size → one block
    {
        std::vector<std::uint8_t> data(7, 0xAB);
        auto blocks = slice_blocks(ByteSpan(data), 7);
        assert(blocks.size() == 1);
        assert(blocks[0].data.size() == 7);
        assert(blocks[0].data == data);
    }

    // 4. Input non-multiple of block_size → multiple blocks with correct sizes
    {
        std::vector<std::uint8_t> data(10, 0);
        for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<std::uint8_t>(i);
        auto blocks = slice_blocks(ByteSpan(data), 3);
        assert(blocks.size() == 4);
        assert(blocks[0].data.size() == 3);
        assert(blocks[1].data.size() == 3);
        assert(blocks[2].data.size() == 3);
        assert(blocks[3].data.size() == 1);
        // Verify reconstruction
        auto rebuilt = reassemble(blocks);
        assert(rebuilt == data);
    }

    // 5. BlockStats on known data
    {
        std::vector<std::uint8_t> data = {0, 0, 0, 1, 2, 3, 3, 3};
        auto stats = BlockStats::compute(ByteSpan(data));
        assert(stats.size == 8);
        assert(stats.unique_byte_count == 4);  // 0, 1, 2, 3
        assert(stats.histogram[0] == 3);
        assert(stats.histogram[3] == 3);
        assert(stats.histogram[1] == 1);
        double h = stats.entropy_estimate();
        assert(h > 0.0 && h <= 8.0);
    }

    // 6. Empty block stats
    {
        auto stats = BlockStats::compute(ByteSpan{});
        assert(stats.size == 0);
        assert(stats.unique_byte_count == 0);
        assert(stats.entropy_estimate() == 0.0);
    }

    std::printf("block_slice_test: PASS\n");
    return 0;
}
