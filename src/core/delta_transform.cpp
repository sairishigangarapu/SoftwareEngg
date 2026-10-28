#include "core/delta_transform.hpp"

#include "core/cost_estimator.hpp"

namespace uatc::core {

std::size_t DeltaTransform::estimateCost(ByteSpan in) const {
    if (in.empty()) return 0;
    // Price the transformed bytes the way the coder would price them, by
    // walking the delta stream and grouping equal deltas into runs without
    // materialising it.
    std::size_t cost = 0;
    std::size_t i = 0;
    while (i < in.size) {
        const std::uint8_t d = (i == 0)
            ? in[0]
            : static_cast<std::uint8_t>(in[i] - in[i - 1]);
        std::size_t run = 1;
        while (i + run < in.size) {
            const std::uint8_t next =
                static_cast<std::uint8_t>(in[i + run] - in[i + run - 1]);
            if (next != d || run >= 128) break;
            ++run;
        }
        // Runs of three or more equal delta bytes price as a 2-byte repeat;
        // anything else is a literal run of one control byte plus its bytes.
        if (run >= 3) {
            cost += 2;
        } else {
            cost += run + 1;
        }
        i += run;
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
