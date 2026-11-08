#pragma once

// DAY 18 - Canonical Huffman entropy coder (FR-07, CodecId::Huffman).
//
// The SAD names canonical Huffman as the entropy coder. The payload layout
// is self-describing: a 256-entry code-length table, the original size, and
// the bit-packed data. The decoder rebuilds the same canonical codes from
// the length table alone.

#include "core/entropy_coder.hpp"

namespace uatc::core {

class HuffmanCoder : public EntropyCoder {
 public:
    CodecId id() const override { return CodecId::Huffman; }
    const char* name() const override { return "Huffman"; }

    CodecResult encode(ByteSpan in) const override;
    ByteBuffer decode(ByteSpan in, std::size_t orig_size) const override;
};

}  // namespace uatc::core
