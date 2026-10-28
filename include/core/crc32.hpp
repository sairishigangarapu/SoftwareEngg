#pragma once

// DAY 11 - CRC-32 (IEEE 802.3), the per-block checksum the SAD specifies.
//
// Reflected algorithm, polynomial 0xEDB88320, initial value 0xFFFFFFFF and a
// final XOR with 0xFFFFFFFF - the same CRC32 that zlib, PNG and gzip use, so
// the values are comparable with any standard implementation.

#include <cstddef>
#include <cstdint>

namespace uatc::core {

// CRC-32 of a byte range.
std::uint32_t crc32(const std::uint8_t* data, std::size_t n);

// Incremental form, for data that arrives in pieces. Start with the initial
// value and feed each piece in turn.
constexpr std::uint32_t kCrc32Initial = 0xFFFFFFFFu;
std::uint32_t crc32_update(std::uint32_t crc, const std::uint8_t* data,
                           std::size_t n);
constexpr std::uint32_t crc32_finish(std::uint32_t crc) { return ~crc; }

}  // namespace uatc::core
