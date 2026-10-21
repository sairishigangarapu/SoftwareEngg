// DAY 4 - Round-trip tests for transforms: decode(encode(x)) == x.
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>
#include "core/none_transform.hpp"
#include "core/registry.hpp"
#include "archive/archive_format.hpp"

int main() {
    using namespace uatc::core;
    using uatc::TransformId;

    NoneTransform none;

    // 1. Empty input
    {
        ByteBuffer empty;
        auto encoded = none.encode(ByteSpan(empty));
        assert(encoded.data.empty());
        auto decoded = none.decode(ByteSpan(encoded.data), 0);
        assert(decoded.empty());
    }

    // 2. Hand-crafted block: all zeros
    {
        std::vector<std::uint8_t> data(100, 0);
        auto encoded = none.encode(ByteSpan(data));
        assert(encoded.data == data);
        auto decoded = none.decode(ByteSpan(encoded.data), data.size());
        assert(decoded == data);
    }

    // 3. Ramp pattern
    {
        std::vector<std::uint8_t> data(256);
        for (int i = 0; i < 256; ++i) data[i] = static_cast<std::uint8_t>(i);
        auto encoded = none.encode(ByteSpan(data));
        auto decoded = none.decode(ByteSpan(encoded.data), data.size());
        assert(decoded == data);
    }

    // 4. Mixed content
    {
        std::vector<std::uint8_t> data = {0,0,0,0,1,2,3,4,4,4,4,4,9,8,7,6,5,5,5,5,5,5};
        auto encoded = none.encode(ByteSpan(data));
        auto decoded = none.decode(ByteSpan(encoded.data), data.size());
        assert(decoded == data);
    }

    // 5. Random data (large)
    {
        std::mt19937 rng(42);
        std::vector<std::uint8_t> data(10000);
        for (auto& b : data) b = static_cast<std::uint8_t>(rng());
        auto encoded = none.encode(ByteSpan(data));
        auto decoded = none.decode(ByteSpan(encoded.data), data.size());
        assert(decoded == data);
    }

    // 6. Registry: NoneTransform registered
    {
        TransformRegistry reg;
        reg.register_transform(std::make_unique<NoneTransform>());
        assert(reg.get(TransformId::None) != nullptr);
        assert(reg.get(TransformId::None)->name() != nullptr);
        assert(reg.get(TransformId::RLE) == nullptr);  // not registered yet
    }

    // 7. Transform interface identity
    {
        assert(none.id() == TransformId::None);
        assert(none.name() != nullptr);
        std::vector<std::uint8_t> data = {42, 43, 44};
        assert(none.estimateCost(ByteSpan(data)) == data.size());
    }

    std::printf("transform_test: PASS\n");
    return 0;
}
