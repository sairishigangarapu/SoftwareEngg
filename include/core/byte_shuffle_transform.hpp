#pragma once

// DAY 7 - ByteShuffleTransform: transpose the byte matrix.
//
// The block is viewed as rows of kByteShuffleStride bytes. The transform
// writes column 0 of every row, then column 1, and so on. Grouping equal
// byte positions together turns multi-byte structured data (int32 tables,
// records with fixed layout) into streams that compress well.
//
// Reversible: the stride is a fixed constant, so no metadata is needed.

#include "core/transform.hpp"

namespace uatc::core {

// Number of bytes per row in the shuffled matrix.
constexpr std::size_t kByteShuffleStride = 16;

class ByteShuffleTransform : public Transform {
 public:
    TransformId id() const override { return TransformId::ByteShuffle; }
    const char* name() const override { return "ByteShuffle"; }

    std::size_t estimateCost(ByteSpan in) const override;

    TransformResult encode(ByteSpan in) const override;
    ByteBuffer decode(ByteSpan in, std::uint32_t params) const override;
};

}  // namespace uatc::core
