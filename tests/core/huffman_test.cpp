// DAY 18 - Canonical Huffman coder tests (FR-07).
//
// Round-trips over the data shapes that matter, the degenerate cases, and
// the property the whole exercise is for: text finally compresses.
#include <cassert>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "core/huffman_coder.hpp"
#include "archive/archive_format.hpp"

int main() {
    using namespace uatc;
    using namespace uatc::core;

    HuffmanCoder coder;

    // 1. Empty input.
    {
        ByteBuffer empty;
        auto encoded = coder.encode(ByteSpan(empty));
        auto decoded = coder.decode(ByteSpan(encoded.data), 0);
        assert(decoded.empty());
    }

    // 2. A single distinct symbol: the degenerate one-bit code.
    {
        std::vector<std::uint8_t> data(10000, 0x7E);
        auto encoded = coder.encode(ByteSpan(data));
        auto decoded = coder.decode(ByteSpan(encoded.data), data.size());
        assert(decoded == data);
        // One symbol: 1 bit each plus the 264-byte header.
        assert(encoded.data.size() < data.size() / 4);
    }

    // 3. Random data round-trips (Huffman will not shrink it - that is the
    //    selector's job, not the coder's).
    {
        std::mt19937 rng(1);
        std::vector<std::uint8_t> data(20000);
        for (auto& b : data) b = static_cast<std::uint8_t>(rng());
        auto encoded = coder.encode(ByteSpan(data));
        auto decoded = coder.decode(ByteSpan(encoded.data), data.size());
        assert(decoded == data);
    }

    // 4. Skewed data compresses well below its size.
    {
        std::vector<std::uint8_t> data(50000);
        std::mt19937 rng(2);
        for (auto& b : data) b = static_cast<std::uint8_t>((rng() % 32 == 0) ? rng() & 0xFF : 0x61);
        auto encoded = coder.encode(ByteSpan(data));
        auto decoded = coder.decode(ByteSpan(encoded.data), data.size());
        assert(decoded == data);
        assert(encoded.data.size() < data.size() / 2);
    }

    // 5. English text: the case Baseline could not compress.
    {
        const std::string text = "the quick brown fox jumps over the lazy dog ";
        std::string big;
        while (big.size() < 60000) big += text;
        std::vector<std::uint8_t> data(big.begin(), big.end());

        auto encoded = coder.encode(ByteSpan(data));
        auto decoded = coder.decode(ByteSpan(encoded.data), data.size());
        assert(decoded == data);
        assert(encoded.data.size() < data.size() / 2);
    }

    // 6. All 256 byte values, uniform.
    {
        std::vector<std::uint8_t> data;
        for (int rep = 0; rep < 100; ++rep)
            for (int v = 0; v < 256; ++v)
                data.push_back(static_cast<std::uint8_t>(v));
        auto encoded = coder.encode(ByteSpan(data));
        auto decoded = coder.decode(ByteSpan(encoded.data), data.size());
        assert(decoded == data);
    }

    // 7. Every length from 0 to 600 round-trips (bit-packing boundaries).
    {
        std::mt19937 rng(3);
        for (std::size_t n = 0; n <= 600; ++n) {
            std::vector<std::uint8_t> data(n);
            for (auto& b : data) b = static_cast<std::uint8_t>(rng() & 0x0F);
            auto encoded = coder.encode(ByteSpan(data));
            auto decoded = coder.decode(ByteSpan(encoded.data), n);
            assert(decoded == data);
        }
    }

    // 8. Determinism: the same input, the same payload.
    {
        std::vector<std::uint8_t> data(3000);
        std::mt19937 rng(4);
        for (auto& b : data) b = static_cast<std::uint8_t>(rng() % 5);
        auto a = coder.encode(ByteSpan(data));
        auto b = coder.encode(ByteSpan(data));
        assert(a.data == b.data);
    }

    // 9. The payload is self-describing: the codec id is Huffman.
    {
        std::vector<std::uint8_t> data = {1, 2, 3};
        auto encoded = coder.encode(ByteSpan(data));
        assert(encoded.codec_id == CodecId::Huffman);
        assert(encoded.original_size == 3);
        assert(encoded.is_valid);
    }

    std::printf("huffman_test: PASS\n");
    return 0;
}
