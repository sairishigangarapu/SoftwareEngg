#pragma once

// DAY 6 - XorTransform: byte-wise XOR against the previous byte.
//
// Reversible: out[i] = in[i] ^ in[i-1]. Useful for data with a repeating
// pattern where XOR against the previous byte flattens the high bits.

#include "core/transform.hpp"

namespace uatc::core {

class XorTransform : public Transform {
 public:
    TransformId id() const override { return TransformId::Xor; }
    const char* name() const override { return "XOR"; }

    std::size_t estimateCost(ByteSpan in) const override;

    TransformResult encode(ByteSpan in) const override;
    ByteBuffer decode(ByteSpan in, std::uint32_t params) const override;
};

}  // namespace uatc::core
