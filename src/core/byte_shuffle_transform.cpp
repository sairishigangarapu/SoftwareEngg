
#include "core/byte_shuffle_transform.hpp"

#include "core/cost_estimator.hpp"

namespace uatc::core {
namespace {

// Number of rows needed to hold n bytes at the given stride.
inline std::size_t row_count(std::size_t n) {
    if (n == 0) {
        return 0;
    }
    return (n + kByteShuffleStride - 1) / kByteShuffleStride;
}

}  // namespace

std::size_t ByteShuffleTransform::estimateCost(ByteSpan in) const {
    if (in.empty()) {
        return 0;
    }

    const std::size_t rows = row_count(in.size);
    std::size_t cost = 0;

    // Estimate repeated runs in shuffled order without materialising it.
    bool have_previous = false;
    std::uint8_t previous = 0;
    std::size_t run_length = 0;

    auto finish_run = [&]() {
        if (run_length >= 3) {
            cost += 2;
        } else {
            cost += run_length;
        }
    };

    for (std::size_t col = 0; col < kByteShuffleStride; ++col) {
        for (std::size_t r = 0; r < rows; ++r) {
            const std::size_t idx = r * kByteShuffleStride + col;

            if (idx >= in.size) {
                continue;
            }

            const std::uint8_t value = in[idx];

            if (!have_previous) {
                previous = value;
                run_length = 1;
                have_previous = true;
            } else if (value == previous && run_length < 128) {
                ++run_length;
            } else {
                finish_run();
                previous = value;
                run_length = 1;
            }

            // Match the shared estimator's maximum run length.
            if (run_length == 128) {
                finish_run();
                have_previous = false;
                run_length = 0;
            }
        }
    }

    if (have_previous) {
        finish_run();
    }

    return cost + kTransformMetadataOverhead;
}

TransformResult ByteShuffleTransform::encode(ByteSpan in) const {
    TransformResult result;

    if (in.empty()) {
        return result;
    }

    const std::size_t rows = row_count(in.size);
    result.data.reserve(in.size);

    for (std::size_t col = 0; col < kByteShuffleStride; ++col) {
        for (std::size_t r = 0; r < rows; ++r) {
            const std::size_t idx = r * kByteShuffleStride + col;

            if (idx < in.size) {
                result.data.push_back(in[idx]);
            }
        }
    }

    return result;
}

ByteBuffer ByteShuffleTransform::decode(
    ByteSpan in, std::uint32_t /*params*/) const {
    ByteBuffer out;

    if (in.empty()) {
        return out;
    }

    const std::size_t rows = row_count(in.size);
    out.resize(in.size);

    std::size_t src = 0;

    for (std::size_t col = 0; col < kByteShuffleStride; ++col) {
        for (std::size_t r = 0; r < rows; ++r) {
            const std::size_t idx = r * kByteShuffleStride + col;

            if (idx < in.size) {
                out[idx] = in[src++];
            }
        }
    }

    return out;
}

}  // namespace uatc::core
