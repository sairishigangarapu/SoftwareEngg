#include "core/block_slicer.hpp"

#include <algorithm>

namespace uatc::core {

std::vector<Block> slice_blocks(ByteSpan input, std::size_t block_size) {
    std::vector<Block> blocks;
    if (block_size == 0 || input.empty()) {
        return blocks;
    }

    std::size_t offset = 0;
    std::size_t id = 0;
    while (offset < input.size) {
        std::size_t current = std::min(block_size, input.size - offset);
        Block b;
        b.block_id = id;
        b.offset = offset;
        b.data.assign(input.data + offset, input.data + offset + current);
        blocks.push_back(std::move(b));
        offset += current;
        ++id;
    }
    return blocks;
}

ByteBuffer reassemble(const std::vector<Block>& blocks) {
    ByteBuffer out;
    std::size_t total = 0;
    for (const auto& b : blocks)
        total += b.data.size();
    out.reserve(total);
    for (const auto& b : blocks)
        out.insert(out.end(), b.data.begin(), b.data.end());
    return out;
}

}  // namespace uatc::core
