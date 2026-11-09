#pragma once

#include <cstddef>
#include <cstdint>

namespace uatc::core {

std::uint32_t crc32(const std::uint8_t* data, std::size_t size);

constexpr std::uint32_t kCrc32Initial = 0xFFFFFFFFu;
std::uint32_t crc32_update(
    std::uint32_t crc,
    const std::uint8_t* data,
    std::size_t size
);
constexpr std::uint32_t crc32_finish(std::uint32_t crc) {
    return ~crc;
}

}  // namespace uatc::core
