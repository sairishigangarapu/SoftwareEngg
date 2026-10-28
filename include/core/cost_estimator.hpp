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

// Estimated coded size (bytes) of a transformed block under the baseline
// coder. The pricing mirrors what the coder emits: a literal run of up to
// 128 bytes costs one control byte plus its bytes, and a repeat run of
// three or more identical bytes costs two bytes. It is an exact model of
// the coder for repeated data and an upper bound for literal-heavy data.
std::size_t estimate_coded_size(const std::uint8_t* data, std::size_t n);

// Overhead (bytes) a transform adds to the stored block for its metadata.
constexpr std::size_t kTransformMetadataOverhead = 1;

}  // namespace uatc::core
