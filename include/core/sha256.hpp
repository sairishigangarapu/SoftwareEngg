#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace uatc::core {

constexpr std::size_t kSha256Size = 32;
using Sha256 = std::array<std::uint8_t, kSha256Size>;

struct Sha256State {
    std::uint32_t h[8];
    std::uint64_t total_bits = 0;
    std::uint8_t buffer[64];
    std::size_t buffered = 0;
};

Sha256State sha256_begin();
void sha256_feed(Sha256State& state, const std::uint8_t* data, std::size_t size);
Sha256 sha256_end(const Sha256State& state);
Sha256 sha256(const std::uint8_t* data, std::size_t size);
std::string sha256_hex(const Sha256& digest);
Sha256 sha256_of_file_prefix(
    const std::filesystem::path& path,
    std::uint64_t size
);

}  // namespace uatc::core
