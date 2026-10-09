#include "archive/codec.hpp"

#include <algorithm>
#include <stdexcept>

namespace uatc {
namespace {
std::size_t run_length(const std::vector<std::uint8_t>& in, std::size_t pos) {
    std::size_t n = 1;
    while (pos + n < in.size() && in[pos + n] == in[pos] && n < 128) ++n;
    return n;
}
}

std::vector<std::uint8_t> Codec::encode(
    const std::vector<std::uint8_t>& input, CodecId id) {
    if (id == CodecId::None) return input;
    if (id != CodecId::Baseline)
        throw std::invalid_argument("Unsupported codec.");

    // PackBits-style stream:
    // 0..127 => next control+1 bytes are literals.
    // 129..255 => repeat next byte (257-control) times.
    // 128 is reserved and never emitted.
    std::vector<std::uint8_t> out;
    std::size_t i = 0;
    while (i < input.size()) {
        const std::size_t run = run_length(input, i);
        if (run >= 3) {
            out.push_back(static_cast<std::uint8_t>(257 - run));
            out.push_back(input[i]);
            i += run;
            continue;
        }

        const std::size_t start = i;
        i += run;
        while (i < input.size() && i - start < 128) {
            const std::size_t next_run = run_length(input, i);
            if (next_run >= 3) break;
            const std::size_t take = std::min(next_run, 128 - (i - start));
            i += take;
            if (take < next_run) break;
        }
        const std::size_t count = i - start;
        out.push_back(static_cast<std::uint8_t>(count - 1));
        out.insert(out.end(), input.begin() + start, input.begin() + i);
    }
    return out;
}

std::vector<std::uint8_t> Codec::decode(
    const std::vector<std::uint8_t>& input, CodecId id) {
    if (id == CodecId::None) return input;
    if (id != CodecId::Baseline)
        throw std::runtime_error("Unsupported codec.");

    std::vector<std::uint8_t> out;
    std::size_t i = 0;
    while (i < input.size()) {
        const std::uint8_t control = input[i++];
        if (control == 128)
            throw std::runtime_error("Invalid Baseline codec control byte.");
        if (control <= 127) {
            const std::size_t count = static_cast<std::size_t>(control) + 1;
            if (count > input.size() - i)
                throw std::runtime_error("Truncated Baseline literal run.");
            if (count > out.max_size() - out.size())
                throw std::runtime_error("Baseline output is too large.");
            out.insert(out.end(), input.begin() + i, input.begin() + i + count);
            i += count;
        } else {
            if (i >= input.size())
                throw std::runtime_error("Truncated Baseline repeat run.");
            const std::size_t count = 257 - control;
            if (count > out.max_size() - out.size())
                throw std::runtime_error("Baseline output is too large.");
            out.insert(out.end(), count, input[i++]);
        }
    }
    return out;
}

}
