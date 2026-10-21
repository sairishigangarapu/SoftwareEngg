#include "core/cost_estimator.hpp"

namespace uatc::core {

std::size_t estimate_coded_size(const std::uint8_t* data, std::size_t n) {
    if (n == 0) return 0;
    std::size_t cost = 0;
    std::size_t i = 0;
    while (i < n) {
        std::size_t run = 1;
        while (i + run < n && data[i + run] == data[i] && run < 128) ++run;
        if (run >= 3) {
            cost += 2;  // control byte + repeated value
            i += run;
        } else {
            cost += run;  // literal bytes
            i += run;
        }
    }
    return cost;
}

}  // namespace uatc::core
