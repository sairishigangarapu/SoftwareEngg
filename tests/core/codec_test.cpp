// DAY 5 - Tests for the baseline entropy coder: random data, repetitive data, round-trip.
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>
#include "core/baseline_entropy_coder.hpp"

int main() {
    using namespace uatc::core;

    BaselineEntropyCoder coder;

    // 1. Empty input round-trip
    {
        ByteBuffer empty;
        auto encoded = coder.encode(ByteSpan(empty));
        assert(encoded.codec_id == CodecId::Baseline);
        assert(encoded.data.empty());
        auto decoded = coder.decode(ByteSpan(encoded.data), 0);
        assert(decoded.empty());
    }

    // 2. Random data round-trip
    {
        std::mt19937 rng(12345);
        std::vector<std::uint8_t> data(10000);
        for (auto& b : data) b = static_cast<std::uint8_t>(rng());
        auto encoded = coder.encode(ByteSpan(data));
        auto decoded = coder.decode(ByteSpan(encoded.data), data.size());
        assert(decoded == data);
        // Random data should not shrink significantly (and may grow slightly)
        assert(encoded.data.size() <= data.size() + data.size() / 3 + 1);
    }

    // 3. Repetitive data round-trip + compression
    {
        std::vector<std::uint8_t> data(1000, 0xAA);
        auto encoded = coder.encode(ByteSpan(data));
        auto decoded = coder.decode(ByteSpan(encoded.data), data.size());
        assert(decoded == data);
        // RLE-style compression should make this much smaller
        assert(encoded.data.size() < data.size());
        assert(encoded.data.size() < 20);  // 1 run marker + 1 byte value
    }

    // 4. Mixed data (literals + runs) round-trip
    {
        std::vector<std::uint8_t> data = {
            1, 2, 3,              // 3 literals
            5, 5, 5, 5, 5,        // run of 5s
            7, 8, 9, 9, 9,        // literals + run
            0, 0, 0, 0, 0, 0, 0, 0  // long run
        };
        auto encoded = coder.encode(ByteSpan(data));
        auto decoded = coder.decode(ByteSpan(encoded.data), data.size());
        assert(decoded == data);
    }

    // 5. Single byte
    {
        std::vector<std::uint8_t> data = {42};
        auto encoded = coder.encode(ByteSpan(data));
        assert(encoded.data.size() == 2);  // control byte + literal
        auto decoded = coder.decode(ByteSpan(encoded.data), 1);
        assert(decoded == data);
    }

    // 6. Single symbol repeated
    {
        std::vector<std::uint8_t> data(256, 7);
        auto encoded = coder.encode(ByteSpan(data));
        auto decoded = coder.decode(ByteSpan(encoded.data), data.size());
        assert(decoded == data);
        assert(encoded.data.size() < data.size());
    }

    // 7. CodecResult metadata
    {
        std::vector<std::uint8_t> data = {1, 2, 3};
        auto encoded = coder.encode(ByteSpan(data));
        assert(encoded.codec_id == CodecId::Baseline);
        assert(encoded.original_size == 3);
        assert(encoded.is_valid);
    }

    std::printf("codec_test: PASS\n");
    return 0;
}
