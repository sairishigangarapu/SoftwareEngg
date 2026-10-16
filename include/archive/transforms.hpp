#pragma once

#include <cstdint>
#include <vector>
#include "archive/archive_format.hpp"

namespace uatc {

// Reversible byte-level transforms. Transform output is not a compressed
// representation by itself; it is passed to the selected codec afterwards.
class Transform {
public:
    static std::vector<std::uint8_t> encode(
        const std::vector<std::uint8_t>& input, TransformId id);
    static std::vector<std::uint8_t> decode(
        const std::vector<std::uint8_t>& input, TransformId id);
};

}
