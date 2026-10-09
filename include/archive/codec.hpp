#pragma once

#include <cstdint>
#include <vector>
#include "archive/archive_format.hpp"

namespace uatc {

// Baseline is a small PackBits-style lossless byte codec.
// Encoded data is used only when selected by the caller.
class Codec {
public:
    static std::vector<std::uint8_t> encode(
        const std::vector<std::uint8_t>& input, CodecId id);
    static std::vector<std::uint8_t> decode(
        const std::vector<std::uint8_t>& input, CodecId id);
};

}
