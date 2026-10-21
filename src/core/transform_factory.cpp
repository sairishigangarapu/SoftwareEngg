#include "core/transform_factory.hpp"

#include "core/byte_shuffle_transform.hpp"
#include "core/delta_transform.hpp"
#include "core/none_transform.hpp"
#include "core/rle_transform.hpp"
#include "core/xor_transform.hpp"

namespace uatc::core {

std::unique_ptr<Transform> make_transform(TransformId id) {
    switch (id) {
    case TransformId::None:
        return std::make_unique<NoneTransform>();
    case TransformId::Delta:
        return std::make_unique<DeltaTransform>();
    case TransformId::Xor:
        return std::make_unique<XorTransform>();
    case TransformId::ByteShuffle:
        return std::make_unique<ByteShuffleTransform>();
    case TransformId::RLE:
        return std::make_unique<RleTransform>();
    default:
        return nullptr;
    }
}

std::vector<TransformId> candidate_transform_ids() {
    return {TransformId::None, TransformId::Delta, TransformId::Xor,
            TransformId::ByteShuffle, TransformId::RLE};
}

TransformRegistry make_default_registry() {
    TransformRegistry registry;
    for (TransformId id : candidate_transform_ids()) {
        auto t = make_transform(id);
        if (t) registry.register_transform(std::move(t));
    }
    return registry;
}

}  // namespace uatc::core
