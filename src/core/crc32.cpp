#include "core/crc32.hpp"

#include <array>

namespace uatc::core {
namespace {

// The 256-entry table for the reflected polynomial 0xEDB88320, built once at
// startup so there is no hand-maintained constant table to get wrong.
struct Table {
    std::array<std::uint32_t, 256> value{};

    Table() {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t crc = i;
            for (int bit = 0; bit < 8; ++bit) {
                crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
            }
            value[i] = crc;
        }
    }
};

const Table& table() {
    static const Table t;
    return t;
}

}  // namespace

std::uint32_t crc32_update(std::uint32_t crc, const std::uint8_t* data,
                           std::size_t n) {
    const auto& t = table();
    for (std::size_t i = 0; i < n; ++i) {
        crc = t.value[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc;
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t n) {
    return crc32_finish(crc32_update(kCrc32Initial, data, n));
}

}  // namespace uatc::core
