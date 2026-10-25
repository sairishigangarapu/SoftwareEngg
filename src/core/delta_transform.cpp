#include "core/delta_transform.hpp"

#include "core/cost_estimator.hpp"

namespace uatc::core {

std::size_t DeltaTransform::estimateCost(ByteSpan in) const {
    if (in.empty()) return 0;
    // Count the bytes that survive as non-zero deltas: those are the ones the
    // entropy coder still has to spend bytes on.
    std::size_t cost = (in[0] == 0) ? 0 : 1;
    for (std::size_t i = 1; i < in.size; ++i) {
        const std::uint8_t d = static_cast<std::uint8_t>(in[i] - in[i - 1]);
        cost += (d == 0) ? 0 : 1;
    }
    return cost + kTransformMetadataOverhead;
}

TransformResult DeltaTransform::encode(ByteSpan in) const {
    TransformResult result;
    result.data.reserve(in.size);
    if (in.empty()) return result;
    result.data.push_back(in[0]);
    for (std::size_t i = 1; i < in.size; ++i)
        result.data.push_back(static_cast<std::uint8_t>(in[i] - in[i - 1]));
    return result;
}

ByteBuffer DeltaTransform::decode(ByteSpan in, std::uint32_t /*params*/) const {
    ByteBuffer out;
    out.reserve(in.size);
    if (in.empty()) return out;
    out.push_back(in[0]);
    for (std::size_t i = 1; i < in.size; ++i)
        out.push_back(static_cast<std::uint8_t>(out.back() + in[i]));
    return out;
}

}  // namespace uatc::core
