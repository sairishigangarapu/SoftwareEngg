
 // DAY 9 - Adaptive transform selection tests (FR-06, FR-08, TC-CORE-03/04/08).
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>

#include "core/transform_factory.hpp"
#include "core/transform_selector.hpp"

int main() {
    using namespace uatc;
    using namespace uatc::core;

    auto registry = make_default_registry();
    TransformSelector selector(registry);

    // 1. Numeric ramp -> Delta is expected to win (small deltas).
    {
        std::vector<std::uint8_t> ramp(4000);
        for (std::size_t i = 0; i < ramp.size(); ++i) {
            ramp[i] = static_cast<std::uint8_t>(i);
        }

        auto s = selector.select(ByteSpan(ramp));
        assert(s.transform_id == TransformId::Delta);
        assert(!s.stored_raw);

        auto* t = registry.get(s.transform_id);
        auto decoded = t->decode(
            ByteSpan(s.transformed.data), s.transformed.params);
        assert(decoded == ramp);
    }

    // 2. Long run of identical bytes -> RLE wins.
    {
        std::vector<std::uint8_t> run(4000, 0x5A);
        auto s = selector.select(ByteSpan(run));
        assert(s.transform_id == TransformId::RLE);

        auto* t = registry.get(s.transform_id);
        auto decoded = t->decode(
            ByteSpan(s.transformed.data), s.transformed.params);
        assert(decoded == run);
    }

    // 3. Random data -> stored unchanged (FR-08 store decision).
    {
        std::mt19937 rng(31337);
        std::vector<std::uint8_t> data(4000);
        for (auto& b : data) {
            b = static_cast<std::uint8_t>(rng());
        }

        auto s = selector.select(ByteSpan(data));
        assert(s.stored_raw);
        assert(s.transform_id == TransformId::None);
        assert(s.transformed.data == data);
    }

    // 4. Ties resolve to the lowest transform id.
    {
        std::vector<std::uint8_t> empty;
        auto s = selector.select(ByteSpan(empty));
        assert(s.transform_id == TransformId::None);
    }

    // 5. Repeated records -> some transform must be selected and round-trip.
    {
        std::vector<std::uint8_t> records;
        for (int i = 0; i < 200; ++i) {
            records.push_back(0x01);
            records.push_back(0x02);
            records.push_back(0x03);
            records.push_back(0x04);
        }

        auto s = selector.select(ByteSpan(records));
        assert(s.estimated_cost <= records.size());

        auto* t = registry.get(s.transform_id);
        auto decoded = t->decode(
            ByteSpan(s.transformed.data), s.transformed.params);
        assert(decoded == records);
    }

    // 6. Selection is deterministic.
    {
        std::vector<std::uint8_t> data(1000);
        for (std::size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<std::uint8_t>((i * 3) & 0xFF);
        }

        auto a = selector.select(ByteSpan(data));
        auto b = selector.select(ByteSpan(data));

        assert(a.transform_id == b.transform_id);
        assert(a.estimated_cost == b.estimated_cost);
        assert(a.transformed.data == b.transformed.data);
    }

    // 7. Mixed file: different blocks get different transforms.
    {
        std::vector<std::uint8_t> text_block(1000, 0x41);
        std::vector<std::uint8_t> numeric_block(1000);

        for (std::size_t i = 0; i < numeric_block.size(); ++i) {
            numeric_block[i] = static_cast<std::uint8_t>(i);
        }

        auto s1 = selector.select(ByteSpan(text_block));
        auto s2 = selector.select(ByteSpan(numeric_block));
        assert(s1.transform_id != s2.transform_id);
    }

    // 8. Every selected transform round-trips its own output.
    {
        std::mt19937 rng(5);

        for (int trial = 0; trial < 20; ++trial) {
            std::vector<std::uint8_t> data(1 + (rng() % 2000));
            for (auto& b : data) {
                b = static_cast<std::uint8_t>(rng() & 0x0F);
            }

            auto s = selector.select(ByteSpan(data));
            auto* t = registry.get(s.transform_id);
            assert(t != nullptr);

            auto decoded = t->decode(
                ByteSpan(s.transformed.data), s.transformed.params);
            assert(decoded == data);
        }
    }

    std::printf("selector_test: PASS\n");
    return 0;
}
