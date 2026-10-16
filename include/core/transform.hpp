#pragma once

// DAY 1 - Transform interface (Strategy pattern).
// A Transform is a reversible byte-level operation applied to a block before
// entropy coding. New transforms implement this interface and register
// themselves with the TransformRegistry — no changes to the compressor logic.

#include <cstdint>
#include <cstddef>
#include "core/byte_buffer.hpp"
#include "archive/archive_format.hpp"

namespace uatc::core {

struct TransformResult {
    ByteBuffer data;       // transformed bytes
    std::uint32_t params = 0;  // transform-specific metadata (fits BlockRecord::transform_parameter)
    bool is_valid = true;
};

class Transform {
 public:
    virtual ~Transform() = default;

    virtual TransformId id() const = 0;
    virtual const char* name() const = 0;

    // Fast, deterministic cost estimate (bytes). Used for transform selection.
    virtual std::size_t estimateCost(ByteSpan in) const = 0;

    // Encode: apply transform to input, write result to out, set params.
    virtual TransformResult encode(ByteSpan in) const = 0;

    // Decode: reverse the transform using params, write original to out.
    virtual ByteBuffer decode(ByteSpan in, std::uint32_t params) const = 0;
};

}  // namespace uatc::core
