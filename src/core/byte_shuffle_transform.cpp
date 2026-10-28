#include "core/byte_shuffle_transform.hpp"

#include <vector>

#include "core/cost_estimator.hpp"

namespace uatc::core {
namespace {

// Number of rows needed to hold n bytes at the given stride.
inline std::size_t row_count(std::size_t n) {
    if (n == 0) return 0;
    return (n + kByteShuffleStride - 1) / kByteShuffleStride;
}

}  // namespace

std::size_t ByteShuffleTransform::estimateCost(ByteSpan in) const {
    if (in.empty()) return 0;
    // The shuffled bytes are a permutation of the input, so the run
    // structure is not visible from the input order. Materialise the
    // shuffled order into a reusable per-thread scratch and price it.
    static thread_local std::vector<std::uint8_t> scratch;
    const std::size_t rows = row_count(in.size);
    scratch.resize(in.size);

    std::size_t src = 0;
    for (std::size_t col = 0; col < kByteShuffleStride; ++col) {
        for (std::size_t r = 0; r < rows; ++r) {
            const std::size_t idx = r * kByteShuffleStride + col;
            if (idx < in.size) scratch[src++] = in[idx];
        }
    }

    return estimate_coded_size(scratch.data(), scratch.size()) +
           kTransformMetadataOverhead;
}

TransformResult ByteShuffleTransform::encode(ByteSpan in) const {
    TransformResult result;
    if (in.empty()) return result;
    const std::size_t rows = row_count(in.size);
    result.data.reserve(in.size);
    for (std::size_t col = 0; col < kByteShuffleStride; ++col) {
        for (std::size_t r = 0; r < rows; ++r) {
            const std::size_t idx = r * kByteShuffleStride + col;
            if (idx < in.size) result.data.push_back(in[idx]);
        }
    }
    return result;
}

ByteBuffer ByteShuffleTransform::decode(ByteSpan in, std::uint32_t /*params*/) const {
    ByteBuffer out;
    if (in.empty()) return out;
    const std::size_t rows = row_count(in.size);
    out.resize(in.size);
    std::size_t src = 0;
    for (std::size_t col = 0; col < kByteShuffleStride; ++col) {
        for (std::size_t r = 0; r < rows; ++r) {
            const std::size_t idx = r * kByteShuffleStride + col;
            if (idx < in.size) out[idx] = in[src++];
        }
    }
    return out;
}

}  // namespace uatc::core
