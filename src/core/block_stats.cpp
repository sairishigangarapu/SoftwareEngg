#include "core/block_stats.hpp"

#include <cmath>

namespace uatc::core {

BlockStats BlockStats::compute(ByteSpan data) {
    BlockStats s;
    s.size = data.size;
    s.unique_byte_count = 0;
    for (std::size_t i = 0; i < data.size; ++i) {
        if (s.histogram[data[i]] == 0)
            ++s.unique_byte_count;
        ++s.histogram[data[i]];
    }
    return s;
}

// Cheap entropy estimate in bits/byte using Shannon entropy of the histogram.
double BlockStats::entropy_estimate() const {
    if (size == 0) return 0.0;
    double h = 0.0;
    for (std::uint32_t c : histogram) {
        if (c > 0) {
            double p = static_cast<double>(c) / static_cast<double>(size);
            h -= p * std::log2(p);
        }
    }
    return h;
}

}  // namespace uatc::core
