#pragma once

// DAY 2/3 - Block statistics: size, unique-byte count, byte-frequency histogram.
#include <array>
#include <cstddef>
#include <cstdint>
#include "core/byte_buffer.hpp"

namespace uatc::core {

struct BlockStats {
    std::size_t size = 0;
    std::size_t unique_byte_count = 0;
    std::array<std::uint32_t, 256> histogram{};

    // Entropy estimate in bits/byte (0–8), cheap lower bound for the probe.
    double entropy_estimate() const;

    static BlockStats compute(ByteSpan data);
};

}  // namespace uatc::core
