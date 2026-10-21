#pragma once

// DAY 9 - Adaptive transform selection.
//
// For each candidate transform the engine estimates the coded cost and picks
// the cheapest one. Ties are broken by the lowest transform id, so the
// selection is deterministic and independent of the worker count (ADR-05).
// If no transform beats storing the block unchanged, the block is stored
// (FR-08).

#include <cstddef>
#include <cstdint>
#include "archive/archive_format.hpp"
#include "core/byte_buffer.hpp"
#include "core/registry.hpp"
#include "core/transform.hpp"

namespace uatc::core {

struct SelectionResult {
    TransformId transform_id = TransformId::None;
    TransformResult transformed;
    std::size_t estimated_cost = 0;  // bytes the block is expected to occupy
    std::size_t original_size = 0;
    bool stored_raw = false;  // true when the block is kept unchanged
};

class TransformSelector {
 public:
    explicit TransformSelector(const TransformRegistry& registry)
        : registry_(registry) {}

    // Picks the cheapest transform for the block.
    SelectionResult select(ByteSpan in) const;

 private:
    const TransformRegistry& registry_;
};

}  // namespace uatc::core
