#pragma once

// DAY 4 - Transform registry.
// Transforms register themselves by TransformId. The compressor looks up
// transforms by ID (or iterates all) without hard-coding the candidate set,
// so new transforms can be added without changing compressor logic.
#include <map>
#include <memory>
#include <vector>
#include "archive/archive_format.hpp"
#include "core/transform.hpp"

namespace uatc::core {

class TransformRegistry {
 public:
    void register_transform(std::unique_ptr<Transform> t) {
        auto id = t->id();
        transforms_[id] = std::move(t);
    }

    Transform* get(TransformId id) const {
        auto it = transforms_.find(id);
        return it == transforms_.end() ? nullptr : it->second.get();
    }

    std::vector<Transform*> all() const {
        std::vector<Transform*> out;
        out.reserve(transforms_.size());
        for (auto& kv : transforms_)
            out.push_back(kv.second.get());
        return out;
    }

    std::vector<TransformId> ids() const {
        std::vector<TransformId> out;
        out.reserve(transforms_.size());
        for (auto& kv : transforms_)
            out.push_back(kv.first);
        return out;
    }

 private:
    std::map<TransformId, std::unique_ptr<Transform>> transforms_;
};

}  // namespace uatc::core
