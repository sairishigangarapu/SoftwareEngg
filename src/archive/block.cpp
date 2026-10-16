
#include "archive/block.hpp"

#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace uatc {

std::vector<Block> BlockSplitter::split(
    const std::vector<std::uint8_t>& data,
    std::size_t block_size
) {
    if (block_size == 0) {
        throw std::invalid_argument(
            "Block size must be greater than zero."
        );
    }

    std::vector<Block> blocks;

    if (data.empty()) {
        return blocks;
    }

    std::size_t offset = 0;
    std::uint32_t block_id = 0;

    while (offset < data.size()) {
        const std::size_t remaining = data.size() - offset;
        const std::size_t current_size =
            (remaining < block_size) ? remaining : block_size;

        Block block;
        block.block_id = block_id;
        block.offset = static_cast<std::uint64_t>(offset);

        block.data.assign(
            data.begin() + offset,
            data.begin() + offset + current_size
        );

        blocks.push_back(std::move(block));

        offset += current_size;
        ++block_id;
    }

    return blocks;
}

} // namespace uatc
