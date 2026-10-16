#pragma once

// DAY 4 - NoneTransform: the identity transform.
// encode returns the original bytes unchanged; decode returns them as-is.
// This is the default/fallback transform when no transform helps.
#include "core/transform.hpp"

namespace uatc::core {

class NoneTransform : public Transform {
 public:
    TransformId id() const override { return TransformId::None; }
    const char* name() const override { return "None"; }

    // None never improves on the input: cost == original size.
    std::size_t estimateCost(ByteSpan in) const override {
        return in.size;
    }

    TransformResult encode(ByteSpan in) const override {
        TransformResult result;
        result.data.assign(in.data, in.data + in.size);
        return result;
    }

    ByteBuffer decode(ByteSpan in, std::uint32_t /*params*/) const override {
        return ByteBuffer(in.data, in.data + in.size);
    }
};

}  // namespace uatc::core
