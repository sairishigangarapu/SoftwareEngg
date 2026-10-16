#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace uatc::core {

// Non-owning byte span for zero-copy input to transforms and coders.
struct ByteSpan {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    ByteSpan() = default;
    ByteSpan(const std::uint8_t* d, std::size_t n) : data(d), size(n) {}
    ByteSpan(const std::vector<std::uint8_t>& v) : data(v.data()), size(v.size()) {}
    const std::uint8_t* begin() const { return data; }
    const std::uint8_t* end() const { return data + size; }
    bool empty() const { return size == 0; }
    const std::uint8_t& operator[](std::size_t i) const { return data[i]; }
};

// Growing byte buffer for transform/coder output.
using ByteBuffer = std::vector<std::uint8_t>;

}  // namespace uatc::core
