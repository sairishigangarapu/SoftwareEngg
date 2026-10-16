#pragma once

// DAY 5 - Baseline entropy coder: PackBits-style RLE over literal/match runs.
// Same algorithm as archive/codec.hpp but behind the EntropyCoder interface,
// so it can be swapped for Huffman/rANS later without touching the compressor.
#include "core/entropy_coder.hpp"

namespace uatc::core {

class BaselineEntropyCoder : public EntropyCoder {
 public:
    CodecId id() const override { return CodecId::Baseline; }
    const char* name() const override { return "baseline"; }

    CodecResult encode(ByteSpan in) const override;
    ByteBuffer decode(ByteSpan in, std::size_t orig_size) const override;

 private:
    static std::size_t run_length(const std::uint8_t* data, std::size_t pos,
                                  std::size_t n);
};

}  // namespace uatc::core
