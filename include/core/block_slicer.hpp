#pragma once

// DAY 3 - Block slicing for byte buffers.
// Splits raw input data into fixed-size blocks. Exact reconstruction is
// guaranteed: concatenating block data in order reproduces the original input.
#include <cstddef>
#include <cstdint>
#include <vector>
#include "core/byte_buffer.hpp"

namespace uatc::core {

struct Block {
    std::size_t block_id = 0;
    std::uint64_t offset = 0;
    ByteBuffer data;
    std::size_t size() const { return data.size(); }
};

// Split input into blocks of at most block_size bytes.
// Empty input produces zero blocks.
std::vector<Block> slice_blocks(ByteSpan input, std::size_t block_size);

// Reassemble blocks into the original byte stream (for verification).
ByteBuffer reassemble(const std::vector<Block>& blocks);

}  // namespace uatc::core
