#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace uatc {

// Represents one raw block of file data.
struct Block {
    std::uint32_t block_id = 0;

    // Position of this block within the original file.
    std::uint64_t offset = 0;

    // Original bytes contained in this block.
    std::vector<std::uint8_t> data;

    std::size_t size() const {
        return data.size();
    }
};

class BlockSplitter {
public:
    // Split input data into blocks of at most block_size bytes.
    static std::vector<Block> split(
        const std::vector<std::uint8_t>& data,
        std::size_t block_size
    );
};

}