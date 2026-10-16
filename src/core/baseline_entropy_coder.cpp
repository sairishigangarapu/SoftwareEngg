#include "core/baseline_entropy_coder.hpp"

#include <algorithm>
#include <stdexcept>

namespace uatc::core {

std::size_t BaselineEntropyCoder::run_length(const std::uint8_t* data,
                                               std::size_t pos, std::size_t n) {
    std::size_t count = 1;
    while (pos + count < n && data[pos + count] == data[pos] && count < 128)
        ++count;
    return count;
}

CodecResult BaselineEntropyCoder::encode(ByteSpan in) const {
    CodecResult result;
    result.codec_id = id();
    result.original_size = in.size;

    // PackBits-style:
    //   0..127  => next (control+1) bytes are literals
    //   129..255 => next byte repeated (257 - control) times
    //   128     => reserved, never emitted
    auto* data = in.data;
    std::size_t n = in.size;
    std::size_t i = 0;
    while (i < n) {
        std::size_t run = run_length(data, i, n);
        if (run >= 3) {
            result.data.push_back(static_cast<std::uint8_t>(257 - run));
            result.data.push_back(data[i]);
            i += run;
            continue;
        }

        // Literal run
        std::size_t start = i;
        i += run;
        while (i < n && (i - start) < 128) {
            std::size_t next_run = run_length(data, i, n);
            if (next_run >= 3) break;
            std::size_t take = std::min(next_run, 128 - (i - start));
            i += take;
            if (take < next_run) break;
        }
        std::size_t count = i - start;
        result.data.push_back(static_cast<std::uint8_t>(count - 1));
        result.data.insert(result.data.end(), data + start, data + i);
    }
    result.is_valid = true;
    return result;
}

ByteBuffer BaselineEntropyCoder::decode(ByteSpan in, std::size_t orig_size) const {
    ByteBuffer out;
    out.reserve(orig_size);

    auto* data = in.data;
    std::size_t n = in.size;
    std::size_t i = 0;
    while (i < n) {
        std::uint8_t control = data[i++];
        if (control == 128)
            throw std::runtime_error("Invalid baseline codec control byte 128");
        if (control <= 127) {
            std::size_t count = static_cast<std::size_t>(control) + 1;
            if (i + count > n)
                throw std::runtime_error("Truncated baseline literal run");
            out.insert(out.end(), data + i, data + i + count);
            i += count;
        } else {
            if (i >= n)
                throw std::runtime_error("Truncated baseline repeat run");
            std::size_t count = 257 - control;
            out.insert(out.end(), count, data[i++]);
        }
    }
    return out;
}

}  // namespace uatc::core
