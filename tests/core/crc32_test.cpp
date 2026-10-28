// DAY 11 - CRC-32 known-answer tests.
//
// The vectors are the standard ones for CRC-32 (IEEE 802.3, reflected,
// polynomial 0xEDB88320) - the same values zlib, PNG and gzip produce - so
// the implementation is verified against the rest of the world, not just
// against itself.
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "core/crc32.hpp"

namespace {

std::uint32_t crc_of(const std::string& s) {
    return uatc::core::crc32(reinterpret_cast<const std::uint8_t*>(s.data()),
                             s.size());
}

}  // namespace

int main() {
    using namespace uatc::core;

    // 1. Known-answer vectors.
    assert(crc_of("") == 0x00000000u);
    assert(crc_of("a") == 0xE8B7BE43u);
    assert(crc_of("abc") == 0x352441C2u);
    assert(crc_of("123456789") == 0xCBF43926u);
    assert(crc_of("The quick brown fox jumps over the lazy dog") == 0x414FA339u);

    // 2. The incremental form agrees with the one-shot form.
    {
        const std::string s = "123456789";
        std::uint32_t crc = kCrc32Initial;
        crc = crc32_update(crc, reinterpret_cast<const std::uint8_t*>(s.data()), 4);
        crc = crc32_update(crc, reinterpret_cast<const std::uint8_t*>(s.data()) + 4, 5);
        assert(crc32_finish(crc) == 0xCBF43926u);
    }

    // 3. One flipped bit in a block changes the CRC.
    {
        std::vector<std::uint8_t> block(4096, 0x5A);
        const auto before = crc32(block.data(), block.size());
        block[123] ^= 0x01;
        const auto after = crc32(block.data(), block.size());
        assert(before != after);
    }

    // 4. The same data always gives the same CRC (deterministic, ADR-05).
    {
        std::vector<std::uint8_t> block(100000);
        for (std::size_t i = 0; i < block.size(); ++i)
            block[i] = static_cast<std::uint8_t>(i & 0xFF);
        assert(crc32(block.data(), block.size()) ==
               crc32(block.data(), block.size()));
    }

    std::printf("crc32_test: PASS\n");
    return 0;
}
