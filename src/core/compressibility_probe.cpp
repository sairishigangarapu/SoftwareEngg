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

    if (sample_n < kProbeMinSize) {
        // Too small for the entropy estimate to mean anything: the maximum
        // entropy of a short block is only log2(n) bits per byte. Fall back
        // to the size estimate, demanding a clear 25% margin so that chance
        // runs of three identical bytes do not read as compressibility.
        result.compressible = estimated + estimated / 4 < sample_n;
        return result;
    }

    // Compressible when the block holds less than 8 bits of information per
    // byte. Random and already-compressed data sit at ~8 and are rejected;
    // text, source code and structured numeric data sit well below.
    result.compressible = result.entropy_bits_per_byte < entropy_threshold_;
    return result;
}

}  // namespace uatc::core
