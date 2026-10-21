#include "core/transform_selector.hpp"

#include <limits>

namespace uatc::core {

SelectionResult TransformSelector::select(ByteSpan in) const {
    SelectionResult result;
    result.original_size = in.size;

    const Transform* best = nullptr;
    std::size_t best_cost = std::numeric_limits<std::size_t>::max();

    // The registry is keyed by TransformId, and std::map iterates in id
    // order, so the first transform with a strictly lower cost wins. Equal
    // costs therefore keep the lowest transform id.
    for (const auto& id : registry_.ids()) {
        const Transform* t = registry_.get(id);
        if (t == nullptr) continue;
        const std::size_t cost = t->estimateCost(in);
        if (cost < best_cost) {
            best_cost = cost;
            best = t;
        }
    }

    if (best == nullptr) {
        // No transform available: store the block unchanged.
        result.transform_id = TransformId::None;
        result.stored_raw = true;
        result.estimated_cost = in.size;
        result.transformed.data.assign(in.data, in.data + in.size);
        return result;
    }

    result.transform_id = best->id();
    result.estimated_cost = best_cost;

    // Store-vs-compress decision (FR-08): the coded form plus its metadata
    // must be smaller than the original for the transform to be worth it.
    if (best_cost >= in.size) {
        result.transform_id = TransformId::None;
        result.stored_raw = true;
        result.estimated_cost = in.size;
        result.transformed.data.assign(in.data, in.data + in.size);
        return result;
    }

    result.transformed = best->encode(in);
    return result;
}

}  // namespace uatc::core
