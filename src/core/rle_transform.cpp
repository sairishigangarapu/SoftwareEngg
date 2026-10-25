#include "core/rle_transform.hpp"

#include <stdexcept>

#include "core/cost_estimator.hpp"

namespace uatc::core {

std::size_t RleTransform::estimateCost(ByteSpan in) const {
    if (in.empty()) return 0;
    // Each run costs 2 bytes regardless of its length.
    std::size_t runs = 0;
    std::size_t i = 0;
    while (i < in.size) {
        std::size_t run = 1;
        while (i + run < in.size && in[i + run] == in[i] && run < 255) ++run;
        ++runs;
        i += run;
    }
    return runs * 2 + kTransformMetadataOverhead;
}

TransformResult RleTransform::encode(ByteSpan in) const {
    TransformResult result;
    if (in.empty()) return result;
    result.data.reserve(in.size * 2);
    std::size_t i = 0;
    while (i < in.size) {
        std::size_t run = 1;
        while (i + run < in.size && in[i + run] == in[i] && run < 255) ++run;
        result.data.push_back(static_cast<std::uint8_t>(run));
        result.data.push_back(in[i]);
        i += run;
    }
    return result;
}

ByteBuffer RleTransform::decode(ByteSpan in, std::uint32_t /*params*/) const {
    ByteBuffer out;
    if (in.empty()) return out;
    if (in.size % 2 != 0)
        throw std::runtime_error("Invalid RLE transform data: odd length.");
    out.reserve(in.size * 2);
    for (std::size_t i = 0; i < in.size; i += 2) {
        const std::size_t count = in[i];
        if (count == 0)
            throw std::runtime_error("Invalid RLE run length of zero.");
        if (count > out.max_size() - out.size())
            throw std::runtime_error("RLE output is too large.");
        out.insert(out.end(), count, in[i + 1]);
    }
    return out;
}

}  // namespace uatc::core
