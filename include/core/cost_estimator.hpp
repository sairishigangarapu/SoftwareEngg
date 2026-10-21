#pragma once

// DAY 6/7 - Shared cost estimation helpers for the transform engine.
//
// The estimate is a fast, deterministic approximation of the coded size of a
// transformed block under the baseline byte coder. It never allocates a coded
// stream, so the compressor can score every candidate transform cheaply
// (ADR-04: selection by fast cost estimate, not by full compression).

#include <cstddef>
#include <cstdint>

namespace uatc::core {

// Estimated coded size (bytes) of a transformed block.
// Runs of 3+ identical bytes cost 2 bytes under the baseline coder
// (control byte + value byte); every other byte costs 1 byte.
std::size_t estimate_coded_size(const std::uint8_t* data, std::size_t n);

// Overhead (bytes) a transform adds to the stored block for its metadata.
constexpr std::size_t kTransformMetadataOverhead = 1;

}  // namespace uatc::core
