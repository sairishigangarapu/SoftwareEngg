#include "core/compressibility_probe.hpp"

#include <cmath>

#include "core/block_stats.hpp"
#include "core/cost_estimator.hpp"

namespace uatc::core {

ProbeResult CompressibilityProbe::probe(ByteSpan in) const {
    ProbeResult result;
    if (in.empty()) {
        // Nothing to gain: an empty block is stored as-is.
        result.compressible = false;
        result.estimated_ratio = 1.0;
        return result;
    }

    // Sample the head of the block; fall back to the whole block when small.
    const std::size_t sample_n = in.size < kProbeSampleSize ? in.size : kProbeSampleSize;
    ByteSpan sample(in.data, sample_n);
    result.sampled_bytes = sample_n;

    const BlockStats stats = BlockStats::compute(sample);
    result.entropy_bits_per_byte = stats.entropy_estimate();

    // Cheap size estimate of the block as-is under the baseline coder.
    const std::size_t estimated = estimate_coded_size(sample.data, sample_n);
    result.estimated_ratio =
        static_cast<double>(estimated) / static_cast<double>(sample_n);

    // Compressible when the estimate beats the original by a clear margin.
    // A 1-byte margin avoids flipping the decision on rounding noise.
    result.compressible =
        (estimated + 1 < sample_n) ||
        (result.entropy_bits_per_byte < entropy_threshold_);
    return result;
}

}  // namespace uatc::core
