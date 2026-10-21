// DAY 8 - Compressibility probe tests (FR-05, TC-CORE-02).
// Text and zeros must be flagged compressible; random and already-compressed
// data must not.
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>

#include "core/compressibility_probe.hpp"

int main() {
    using namespace uatc::core;

    CompressibilityProbe probe;

    // 1. Empty input is not worth compressing.
    {
        std::vector<std::uint8_t> empty;
        auto r = probe.probe(ByteSpan(empty));
        assert(!r.compressible);
        assert(r.sampled_bytes == 0);
    }

    // 2. All zeros -> highly compressible.
    {
        std::vector<std::uint8_t> zeros(4096, 0);
        auto r = probe.probe(ByteSpan(zeros));
        assert(r.compressible);
        assert(r.entropy_bits_per_byte < 1.0);
        assert(r.estimated_ratio < 0.5);
    }

    // 3. English-like text -> compressible.
    {
        std::string text;
        const char* words[] = {"the", "quick", "brown", "fox", "jumps", "over",
                               "lazy", "dog", "and", "then", "runs", "away"};
        for (int i = 0; i < 4000; ++i) {
            text += words[i % 12];
            text += ' ';
        }
        std::vector<std::uint8_t> data(text.begin(), text.end());
        auto r = probe.probe(ByteSpan(data));
        assert(r.compressible);
        assert(r.entropy_bits_per_byte < 7.0);
    }

    // 4. Random bytes -> not compressible.
    {
        std::mt19937 rng(7);
        std::vector<std::uint8_t> data(4096);
        for (auto& b : data) b = static_cast<std::uint8_t>(rng());
        auto r = probe.probe(ByteSpan(data));
        assert(!r.compressible);
        assert(r.entropy_bits_per_byte > 7.5);
    }

    // 5. Already-compressed-like data (incompressible) -> not compressible.
    {
        std::mt19937 rng(99);
        std::vector<std::uint8_t> data(8192);
        for (auto& b : data) b = static_cast<std::uint8_t>(rng());
        auto r = probe.probe(ByteSpan(data));
        assert(!r.compressible);
    }

    // 6. Large block is sampled, not fully scanned.
    {
        std::vector<std::uint8_t> data(1024 * 1024, 0);
        auto r = probe.probe(ByteSpan(data));
        assert(r.sampled_bytes == kProbeSampleSize);
        assert(r.compressible);
    }

    // 7. Small block is probed in full.
    {
        std::vector<std::uint8_t> data(10, 0);
        auto r = probe.probe(ByteSpan(data));
        assert(r.sampled_bytes == 10);
    }

    // 8. is_compressible convenience wrapper agrees with probe().
    {
        std::vector<std::uint8_t> zeros(1024, 0);
        assert(probe.is_compressible(ByteSpan(zeros)));
    }

    std::printf("probe_test: PASS\n");
    return 0;
}
