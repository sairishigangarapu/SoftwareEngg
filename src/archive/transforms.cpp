#include "archive/transforms.hpp"

#include <stdexcept>

#include "core/transform_factory.hpp"

namespace uatc {

// The archive layer delegates to the compression core's transform engine so
// that there is exactly one implementation of each transform. The transform
// ids and the byte format are shared with the core (see
// archive/archive_format.hpp and core/transform_factory.hpp).

std::vector<std::uint8_t> Transform::encode(
    const std::vector<std::uint8_t>& input, TransformId id) {
    auto t = core::make_transform(id);
    if (!t) throw std::invalid_argument("Unsupported transform.");
    return t->encode(core::ByteSpan(input)).data;
}

std::vector<std::uint8_t> Transform::decode(
    const std::vector<std::uint8_t>& input, TransformId id) {
    auto t = core::make_transform(id);
    if (!t) throw std::runtime_error("Unsupported transform.");
    return t->decode(core::ByteSpan(input), 0);
}

}  // namespace uatc
