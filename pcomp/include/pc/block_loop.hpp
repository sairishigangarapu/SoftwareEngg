#pragma once
#include <string>
#include "pc/bench.hpp"
#include "pc/block.hpp"
#include "pc/codec.hpp"

namespace pc {

struct LoopConfig {
  size_t block_size = 1 << 20;
  const Codec* codec = nullptr;
};

// Pure per-block work. THIS is what a pool worker will run later.
BlockResult process_block(const Codec& codec, BlockJob&& job);

// Single-thread loops with read / process / write stage timing.
StageTimes compress_file(const std::string& in, const std::string& out,
                         const LoopConfig& cfg, uint32_t file_index);
StageTimes decompress_file(const std::string& in, const std::string& out, const LoopConfig& cfg);

uint64_t fnv1a_file(const std::string& path);  // cheap round-trip check

}  // namespace pc
