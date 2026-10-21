#include "core/xor_transform.hpp"

#include "core/cost_estimator.hpp"

namespace uatc::core {

std::size_t XorTransform::estimateCost(ByteSpan in) const {
    if (in.empty()) return 0;

    ByteBuffer transformed;
    transformed.reserve(in.size);
    transformed.push_back(in[0]);

    for (std::size_t i = 1; i < in.size; ++i) {
        transformed.push_back(
            static_cast<std::uint8_t>(in[i] ^ in[i - 1])
        );
    }

    return estimate_coded_size(
        transformed.data(), transformed.size()
    ) + kTransformMetadataOverhead;
}

TransformResult XorTransform::encode(ByteSpan in) const {
    TransformResult result;
    result.data.reserve(in.size);
    if (in.empty()) return result;
    result.data.push_back(in[0]);
    for (std::size_t i = 1; i < in.size; ++i)
        result.data.push_back(static_cast<std::uint8_t>(in[i] ^ in[i - 1]));
    return result;
}

ByteBuffer XorTransform::decode(ByteSpan in, std::uint32_t /*params*/) const {
    ByteBuffer out;
    out.reserve(in.size);
    if (in.empty()) return out;
    out.push_back(in[0]);
    for (std::size_t i = 1; i < in.size; ++i)
        out.push_back(static_cast<std::uint8_t>(out.back() ^ in[i]));
    return out;
}

}  // namespace uatc::core
