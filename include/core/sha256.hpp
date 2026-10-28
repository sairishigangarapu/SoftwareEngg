#pragma once

// DAY 12 - SHA-256 (FIPS 180-4), the file-level and archive-level checksum.
//
// The SAD specifies a SHA-256 per file and per archive, in the Integrity
// Data region, so the implementation has to match the standard exactly.

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace uatc::core {

constexpr std::size_t kSha256Size = 32;

// SHA-256 digest as 32 raw bytes.
using Sha256 = std::array<std::uint8_t, kSha256Size>;

// SHA-256 of a byte range.
Sha256 sha256(const std::uint8_t* data, std::size_t n);

// Incremental form. Start with the initial state and feed each piece in turn.
struct Sha256State {
    std::uint32_t h[8];
    std::uint64_t total_bits = 0;
    std::uint8_t buffer[64];
    std::size_t buffered = 0;
};

Sha256State sha256_begin();
void sha256_feed(Sha256State& state, const std::uint8_t* data, std::size_t n);
Sha256 sha256_end(const Sha256State& state);

// Lowercase hex, the form used in logs and reports.
std::string sha256_hex(const Sha256& digest);

// SHA-256 of the first `n` bytes of a file, read back off disk. Used for the
// archive-level checksum, which is taken over everything the writer produced.
Sha256 sha256_of_file_prefix(const std::filesystem::path& path,
                             std::uint64_t n);

}  // namespace uatc::core
