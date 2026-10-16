#pragma once

// DAY 5 - CodecResult: result of entropy-coding a block.
#include <cstddef>
#include <cstdint>
#include "archive/archive_format.hpp"
#include "core/byte_buffer.hpp"

namespace uatc::core {

struct CodecResult {
    ByteBuffer data;             // encoded bytes
    CodecId codec_id = CodecId::None;
    std::size_t original_size = 0;
    bool is_valid = true;
};

}  // namespace uatc::core
