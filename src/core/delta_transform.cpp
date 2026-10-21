
#include "core/delta_transform.hpp"

#include "core/cost_estimator.hpp"

namespace uatc::core {

std::size_t DeltaTransform::estimateCost(ByteSpan in) const {
    if (in.empty()) {
        return 0;
    }

    // Estimate the cost of the actual Delta stream without allocating
    // a temporary buffer.
    auto delta_at = [&in](std::size_t index) -> std::uint8_t {
        if (index == 0) {
            return in[0];
        }

        return static_cast<std::uint8_t>(
            in[index] - in[index - 1]);
    };

    std::size_t cost = 0;
    std::size_t i = 0;

    while (i < in.size) {
        const std::uint8_t value = delta_at(i);
        std::size_t run = 1;

        while (i + run < in.size &&
               run < 128 &&
               delta_at(i + run) == value) {
            ++run;
        }

        if (run >= 3) {
            cost += 2;
        } else {
            cost += run;
        }

        i += run;
    }

    return cost + kTransformMetadataOverhead;
}

TransformResult DeltaTransform::encode(ByteSpan in) const {
    TransformResult result;
    result.data.reserve(in.size);

    if (in.empty()) {
        return result;
    }

    result.data.push_back(in[0]);

    for (std::size_t i = 1; i < in.size; ++i) {
        result.data.push_back(
            static_cast<std::uint8_t>(in[i] - in[i - 1]));
    }

    return result;
}

ByteBuffer DeltaTransform::decode(
    ByteSpan in,
    std::uint32_t /*params*/) const {
    ByteBuffer out;
    out.reserve(in.size);

    if (in.empty()) {
        return out;
    }

    out.push_back(in[0]);

    for (std::size_t i = 1; i < in.size; ++i) {
        out.push_back(
            static_cast<std::uint8_t>(out.back() + in[i]));
    }

    return out;
}

}  // namespace uatc::core
