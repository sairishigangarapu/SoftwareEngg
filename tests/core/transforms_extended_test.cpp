// DAY 6/7 - Extended transform set: Delta, XOR, ByteShuffle, RLE.
// Round-trip: decode(encode(x)) == x for every candidate transform.
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>

#include "core/transform_factory.hpp"
#include "core/byte_shuffle_transform.hpp"
#include "core/delta_transform.hpp"
#include "core/rle_transform.hpp"
#include "core/xor_transform.hpp"

namespace {

void round_trip(uatc::core::Transform& t, const std::vector<std::uint8_t>& data) {
    auto encoded = t.encode(uatc::core::ByteSpan(data));
    auto decoded = t.decode(uatc::core::ByteSpan(encoded.data), encoded.params);
    assert(decoded == data);
}

}  // namespace

int main() {
    using namespace uatc;
    using namespace uatc::core;

    DeltaTransform delta;
    XorTransform xor_t;
    ByteShuffleTransform shuffle;
    RleTransform rle;

    // --- Empty input ---
    for (auto* t : {static_cast<Transform*>(&delta), static_cast<Transform*>(&xor_t),
                    static_cast<Transform*>(&shuffle), static_cast<Transform*>(&rle)}) {
        std::vector<std::uint8_t> empty;
        round_trip(*t, empty);
        assert(t->estimateCost(uatc::core::ByteSpan(empty)) == 0);
    }

    // --- Hand-crafted blocks ---
    std::vector<std::uint8_t> zeros(500, 0);
    std::vector<std::uint8_t> ramp(256);
    for (int i = 0; i < 256; ++i) ramp[i] = static_cast<std::uint8_t>(i);
    std::vector<std::uint8_t> mixed = {0,0,0,0, 1,2,3,4,4,4,4,4, 9,8,7,6,5,5,5,5,5,5};
    std::vector<std::uint8_t> one(1, 0x42);
    std::vector<std::uint8_t> structured(256);
    for (int i = 0; i < 256; ++i)
        structured[i] = static_cast<std::uint8_t>(i % 16);

    for (auto* t : {static_cast<Transform*>(&delta), static_cast<Transform*>(&xor_t),
                    static_cast<Transform*>(&shuffle), static_cast<Transform*>(&rle)}) {
        round_trip(*t, zeros);
        round_trip(*t, ramp);
        round_trip(*t, mixed);
        round_trip(*t, one);
        round_trip(*t, structured);
    }

    // --- Random data ---
    {
        std::mt19937 rng(2024);
        std::vector<std::uint8_t> data(5000);
        for (auto& b : data) b = static_cast<std::uint8_t>(rng());
        round_trip(delta, data);
        round_trip(xor_t, data);
        round_trip(shuffle, data);
        round_trip(rle, data);
    }

    // --- Sizes around the byte-shuffle stride (16) ---
    for (std::size_t n = 0; n <= 40; ++n) {
        std::vector<std::uint8_t> data(n);
        for (std::size_t i = 0; i < n; ++i) data[i] = static_cast<std::uint8_t>(i * 7);
        round_trip(shuffle, data);
        round_trip(delta, data);
        round_trip(rle, data);
    }

    // --- Transform ids match the archive format ---
    assert(delta.id() == TransformId::Delta);
    assert(xor_t.id() == TransformId::Xor);
    assert(shuffle.id() == TransformId::ByteShuffle);
    assert(rle.id() == TransformId::RLE);
    assert(static_cast<std::uint8_t>(TransformId::None) == 0);
    assert(static_cast<std::uint8_t>(TransformId::Delta) == 1);
    assert(static_cast<std::uint8_t>(TransformId::Xor) == 2);
    assert(static_cast<std::uint8_t>(TransformId::ByteShuffle) == 3);
    assert(static_cast<std::uint8_t>(TransformId::RLE) == 4);

    // --- Factory covers the whole candidate set ---
    {
        auto ids = candidate_transform_ids();
        assert(ids.size() == 5);
        for (TransformId id : ids) {
            auto t = make_transform(id);
            assert(t != nullptr);
            assert(t->id() == id);
        }
        assert(make_transform(TransformId::Reserved) == nullptr);
    }

    // --- Default registry holds every candidate ---
    {
        auto registry = make_default_registry();
        assert(registry.ids().size() == 5);
        for (TransformId id : candidate_transform_ids())
            assert(registry.get(id) != nullptr);
    }

    // --- Transform effectiveness sanity ---
    {
        // Delta on a slowly increasing ramp should be much cheaper than raw.
        std::vector<std::uint8_t> slow(2000);
        for (std::size_t i = 0; i < slow.size(); ++i)
            slow[i] = static_cast<std::uint8_t>(i / 4);
        assert(delta.estimateCost(ByteSpan(slow)) < slow.size());

        // RLE on long runs should be much cheaper than raw.
        std::vector<std::uint8_t> run(2000, 0x5A);
        assert(rle.estimateCost(ByteSpan(run)) < run.size());

        // Byte-shuffle on structured data should beat raw.
        assert(shuffle.estimateCost(ByteSpan(structured)) < structured.size());
    }

    std::printf("transforms_extended_test: PASS\n");
    return 0;
}
