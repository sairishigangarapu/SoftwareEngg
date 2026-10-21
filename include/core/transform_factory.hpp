#pragma once

// DAY 7 - Transform factory: the single place that maps a TransformId to an
// implementation. Adding a transform means adding a case here and registering
// it; the compressor, archive layer, CLI and frontend are untouched (ADR:
// modularity, NFR Maintainability).

#include <memory>
#include <vector>
#include "archive/archive_format.hpp"
#include "core/registry.hpp"
#include "core/transform.hpp"

namespace uatc::core {

// Returns the transform for the given id, or nullptr if the id is unknown.
std::unique_ptr<Transform> make_transform(TransformId id);

// Every transform in the v1 candidate set, in stable id order.
std::vector<TransformId> candidate_transform_ids();

// A registry pre-populated with the whole v1 candidate set.
TransformRegistry make_default_registry();

}  // namespace uatc::core
