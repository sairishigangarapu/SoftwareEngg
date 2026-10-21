#pragma once

// DAY 6 - DeltaTransform: byte-wise differencing against the previous byte.
//
// Good for slowly changing data (sorted counters, time series, text with
// small deltas). Reversible: out[i] = in[i] - in[i-1].

#include "core/transform.hpp"

namespace uatc::core {

class DeltaTransform : public Transform {
 public:
    TransformId id() const override { return TransformId::Delta; }
    const char* name() const override { return "Delta"; }

    std::size_t estimateCost(ByteSpan in) const override;

    TransformResult encode(ByteSpan in) const override;
    ByteBuffer decode(ByteSpan in, std::uint32_t params) const override;
};

}  // namespace uatc::core
