#include "core/crc32.hpp"

#include <array>

namespace uatc::core {
namespace {

struct Table {
    std::array<std::uint32_t, 256> values{};

    Table() {
        for (std::uint32_t i = 0; i < values.size(); ++i) {
            std::uint32_t crc = i;
            for (int bit = 0; bit < 8; ++bit) {
                crc = (crc & 1u) != 0
                    ? (crc >> 1) ^ 0xEDB88320u
                    : crc >> 1;
            }
            values[i] = crc;
        }
    }
};

const Table& table() {
    static const Table instance;
    return instance;
}

}  // namespace

std::uint32_t crc32_update(
    std::uint32_t crc,
    const std::uint8_t* data,
    std::size_t size
) {
    const auto& lookup = table().values;
    for (std::size_t i = 0; i < size; ++i) {
        crc = lookup[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc;
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
    return crc32_finish(crc32_update(kCrc32Initial, data, size));
}

}  // namespace uatc::core
