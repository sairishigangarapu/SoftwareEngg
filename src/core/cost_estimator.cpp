#include "core/cost_estimator.hpp"

#include <algorithm>

namespace uatc::core {

std::size_t estimate_coded_size(const std::uint8_t* data, std::size_t n) {
    if (n == 0) return 0;
    // Pricing that mirrors the baseline coder byte for byte: a run of three
    // or more identical bytes becomes a 2-byte repeat; everything else is
    // accumulated into literal runs of up to 128 bytes, each preceded by its
    // own control byte - exactly as the coder's encoder does. For any block,
    // this equals the number of bytes the coder will emit.
    std::size_t cost = 0;
    std::size_t i = 0;
    while (i < n) {
        std::size_t run = 1;
        while (i + run < n && data[i + run] == data[i] && run < 128) ++run;
        if (run >= 3) {
            cost += 2;  // control byte + repeated value
            i += run;
            continue;
        }

        // Literal accumulation, same walk as the encoder.
        const std::size_t start = i;
        i += run;
        while (i < n && (i - start) < 128) {
            std::size_t next_run = 1;
            while (i + next_run < n && data[i + next_run] == data[i] &&
                   next_run < 128) ++next_run;
            if (next_run >= 3) break;
            const std::size_t take = std::min(next_run, 128 - (i - start));
            i += take;
            if (take < next_run) break;
        }
        cost += (i - start) + 1;  // control byte + literal bytes
    }
    return cost;
}

}  // namespace uatc::core
