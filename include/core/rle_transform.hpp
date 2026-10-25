#pragma once

// DAY 7 - RleTransform: run-length encoding as (count, value) pairs.
//
// Self-delimiting: each run occupies exactly 2 bytes, with the count in
// 1..255. The decoder reconstructs the run stream without extra metadata.

#include "core/transform.hpp"

namespace uatc::core {

class RleTransform : public Transform {
 public:
    TransformId id() const override { return TransformId::RLE; }
    const char* name() const override { return "RLE"; }

    std::size_t estimateCost(ByteSpan in) const override;

    TransformResult encode(ByteSpan in) const override;
    ByteBuffer decode(ByteSpan in, std::uint32_t params) const override;
};

}  // namespace uatc::core
