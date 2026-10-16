#include "archive/transforms.hpp"

#include <stdexcept>

namespace uatc {

std::vector<std::uint8_t> Transform::encode(
    const std::vector<std::uint8_t>& input, TransformId id) {
    switch (id) {
    case TransformId::None:
        return input;
    case TransformId::RLE: {
        std::vector<std::uint8_t> out;
        out.reserve(input.size() * 2);
        std::size_t i = 0;
        while (i < input.size()) {
            std::size_t run = 1;
            while (i + run < input.size() && input[i + run] == input[i] &&
                   run < 255) ++run;
            out.push_back(static_cast<std::uint8_t>(run));
            out.push_back(input[i]);
            i += run;
        }
        return out;
    }
    case TransformId::Delta: {
        std::vector<std::uint8_t> out;
        out.reserve(input.size());
        if (input.empty()) return out;
        out.push_back(input.front());
        for (std::size_t i = 1; i < input.size(); ++i)
            out.push_back(static_cast<std::uint8_t>(input[i] - input[i - 1]));
        return out;
    }
    default:
        throw std::invalid_argument("Unsupported transform.");
    }
}

std::vector<std::uint8_t> Transform::decode(
    const std::vector<std::uint8_t>& input, TransformId id) {
    switch (id) {
    case TransformId::None:
        return input;
    case TransformId::RLE: {
        if (input.size() % 2 != 0)
            throw std::runtime_error("Invalid RLE transform data.");
        std::vector<std::uint8_t> out;
        for (std::size_t i = 0; i < input.size(); i += 2) {
            const auto count = input[i];
            if (count == 0)
                throw std::runtime_error("Invalid RLE run length.");
            if (static_cast<std::size_t>(count) >
                out.max_size() - out.size())
                throw std::runtime_error("RLE output is too large.");
            out.insert(out.end(), count, input[i + 1]);
        }
        return out;
    }
    case TransformId::Delta: {
        std::vector<std::uint8_t> out;
        out.reserve(input.size());
        if (input.empty()) return out;
        out.push_back(input.front());
        for (std::size_t i = 1; i < input.size(); ++i)
            out.push_back(static_cast<std::uint8_t>(out.back() + input[i]));
        return out;
    }
    default:
        throw std::runtime_error("Unsupported transform.");
    }
}

}
