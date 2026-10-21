
 // DAY 5 - Tests for the baseline entropy coder: random data, repetitive data, round-trip.
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>

#include "archive/codec.hpp"
#include "core/baseline_entropy_coder.hpp"

int main() {
    using namespace uatc::core;
    using uatc::CodecId;

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

        for (auto& b : data) {
            b = static_cast<std::uint8_t>(rng());
        }

        auto encoded = coder.encode(ByteSpan(data));
        auto decoded = coder.decode(ByteSpan(encoded.data), data.size());

        assert(decoded == data);

        // Random data should not shrink significantly (and may grow slightly).
        assert(encoded.data.size() <= data.size() + data.size() / 3 + 1);
    }

    // 3. Repetitive data round-trip + compression
    {
        std::vector<std::uint8_t> data(1000, 0xAA);

        auto encoded = coder.encode(ByteSpan(data));
        auto decoded = coder.decode(ByteSpan(encoded.data), data.size());

        assert(decoded == data);

        // RLE-style compression should make this much smaller.
        assert(encoded.data.size() < data.size());
        assert(encoded.data.size() < 20);
    }

    // 4. Mixed data (literals + runs) round-trip
    {
        std::vector<std::uint8_t> data = {
            1, 2, 3,                       // 3 literals
            5, 5, 5, 5, 5,                 // run of 5s
            7, 8, 9, 9, 9,                 // literals + run
            0, 0, 0, 0, 0, 0, 0, 0        // long run
        };

        auto encoded = coder.encode(ByteSpan(data));
        auto decoded = coder.decode(ByteSpan(encoded.data), data.size());

        assert(decoded == data);
    }

    // 5. Single byte
    {
        std::vector<std::uint8_t> data = {42};

        auto encoded = coder.encode(ByteSpan(data));

        assert(encoded.data.size() == 2);

        auto decoded = coder.decode(ByteSpan(encoded.data), data.size());
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
        assert(encoded.original_size == data.size());
        assert(encoded.is_valid);
    }

    // 8. Compatibility between core and archive codecs
    {
        std::vector<std::uint8_t> data;

        for (int i = 0; i < 200; ++i) {
            data.push_back(static_cast<std::uint8_t>(i % 17));
        }

        data.insert(data.end(), 300, 0xAA);

        // Core encoder -> archive decoder
        auto core_encoded = coder.encode(ByteSpan(data));

        auto archive_decoded = uatc::Codec::decode(
            core_encoded.data, CodecId::Baseline);

        assert(archive_decoded == data);

        // Archive encoder -> core decoder
        auto archive_encoded = uatc::Codec::encode(
            data, CodecId::Baseline);

        auto core_decoded = coder.decode(
            ByteSpan(archive_encoded), data.size());

        assert(core_decoded == data);

        // Both implementations should emit the same format.
        assert(core_encoded.data == archive_encoded);
    }

    std::printf("codec_test: PASS\n");
    return 0;
}
