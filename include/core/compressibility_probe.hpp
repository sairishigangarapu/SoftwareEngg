#pragma once

// DAY 8 - Compressibility probe.
//
// Cheap estimate, taken on a sample, of whether a block is worth pushing
// through the full compression pipeline. If the estimated coded size is not
// smaller than the original, the block is stored unchanged instead
// (FR-08: store-or-compress decision).

#include <cstddef>
#include <cstdint>
#include "core/byte_buffer.hpp"

namespace uatc::core {

// Bytes sampled from the head of a block for the estimate.
constexpr std::size_t kProbeSampleSize = 64 * 1024;

// Smallest block that is probed on its full content.
constexpr std::size_t kProbeMinSize = 64;

struct ProbeResult {
    bool compressible = false;        // worth running the transform pipeline
    double entropy_bits_per_byte = 0.0;  // Shannon entropy of the sample
    double estimated_ratio = 1.0;     // estimated coded size / original size
    std::size_t sampled_bytes = 0;
};

class CompressibilityProbe {
 public:
    // A block is judged compressible when its Shannon entropy is below this
    // threshold, i.e. it holds less than 8 bits of information per byte.
    // Blocks that are too small for the entropy estimate to be meaningful are
    // judged by their estimated coded size instead.
    explicit CompressibilityProbe(double entropy_threshold = 7.5)
        : entropy_threshold_(entropy_threshold) {}

    ProbeResult probe(ByteSpan in) const;

    bool is_compressible(ByteSpan in) const { return probe(in).compressible; }

 private:
    double entropy_threshold_;
};

}  // namespace uatc::core
