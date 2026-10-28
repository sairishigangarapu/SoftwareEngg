// DAY 12 - SHA-256 known-answer tests, straight from FIPS 180-4.
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "core/sha256.hpp"

namespace {

std::string hex_of(const std::string& s) {
    return uatc::core::sha256_hex(
        uatc::core::sha256(reinterpret_cast<const std::uint8_t*>(s.data()),
                           s.size()));
}

}  // namespace

int main() {
    using namespace uatc::core;

    // 1. The FIPS 180-4 test vectors.
    assert(hex_of("") ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    assert(hex_of("abc") ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    assert(hex_of("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    // 2. The million-'a' vector, fed in pieces through the incremental form.
    {
        std::vector<std::uint8_t> chunk(1000, 'a');
        auto state = sha256_begin();
        for (int i = 0; i < 1000; ++i) sha256_feed(state, chunk.data(), chunk.size());
        assert(sha256_hex(sha256_end(state)) ==
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    }

    // 3. Every input length from 0 to 200 bytes, incremental vs one-shot.
    //    This covers the padding boundary cases: 55, 56, 57, 63, 64, 65...
    {
        std::vector<std::uint8_t> data(200);
        for (std::size_t i = 0; i < data.size(); ++i)
            data[i] = static_cast<std::uint8_t>(i * 7);
        for (std::size_t n = 0; n <= 200; ++n) {
            auto state = sha256_begin();
            // Feed in awkward sizes, not whole blocks.
            std::size_t fed = 0;
            while (fed < n) {
                const std::size_t take = (n - fed < 7) ? n - fed : 7;
                sha256_feed(state, data.data() + fed, take);
                fed += take;
            }
            assert(sha256_end(state) == sha256(data.data(), n));
        }
    }

    // 4. One flipped bit changes the digest.
    {
        std::vector<std::uint8_t> data(4096, 0x3C);
        const auto before = sha256(data.data(), data.size());
        data[2048] ^= 0x80;
        const auto after = sha256(data.data(), data.size());
        assert(before != after);
    }

    // 5. The digest is deterministic.
    {
        std::vector<std::uint8_t> data(10000);
        for (std::size_t i = 0; i < data.size(); ++i)
            data[i] = static_cast<std::uint8_t>(i & 0xFF);
        assert(sha256(data.data(), data.size()) == sha256(data.data(), data.size()));
    }

    std::printf("sha256_test: PASS\n");
    return 0;
}
