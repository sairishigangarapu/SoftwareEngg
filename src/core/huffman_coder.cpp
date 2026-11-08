#include "core/huffman_coder.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace uatc::core {
namespace {

// Payload layout: 256 length bytes, 8 size bytes, then the bit data.
constexpr std::size_t kTableSize = 256;
constexpr std::size_t kHeaderSize = kTableSize + 8;

// Builds canonical code lengths from symbol frequencies: repeated selection
// of the two lightest nodes. At most 511 nodes, so the quadratic scan is
// bounded and keeps the implementation free of a heap.
std::array<std::uint8_t, 256> build_lengths(const std::array<std::uint64_t, 256>& freq,
                                            std::size_t distinct) {
    std::array<std::uint8_t, 256> lengths{};

    if (distinct == 0) return lengths;
    if (distinct == 1) {
        // A single symbol still needs one bit.
        for (int s = 0; s < 256; ++s)
            if (freq[s] > 0) lengths[s] = 1;
        return lengths;
    }

    // Nodes: leaves first, then internal nodes as they are created.
    const std::size_t max_nodes = 2 * distinct;
    std::vector<std::uint64_t> weight(max_nodes, 0);
    std::vector<int> parent(max_nodes, -1);
    std::vector<bool> used(max_nodes, false);
    std::vector<int> symbol_of(max_nodes, -1);

    std::size_t node_count = 0;
    for (int s = 0; s < 256; ++s) {
        if (freq[s] > 0) {
            weight[node_count] = freq[s];
            symbol_of[node_count] = s;
            ++node_count;
        }
    }

    const std::size_t leaves = node_count;
    // A binary tree over `distinct` leaves needs exactly distinct-1 merges,
    // ending at 2*distinct-1 nodes. One iteration more would find no unused
    // node and index the arrays at -1.
    for (std::size_t merges = 0; merges + 1 < distinct; ++merges) {
        // Two lightest unused nodes.
        int first = -1, second = -1;
        for (std::size_t i = 0; i < node_count; ++i) {
            if (used[i]) continue;
            if (first == -1 || weight[i] < weight[first]) {
                second = first;
                first = static_cast<int>(i);
            } else if (second == -1 || weight[i] < weight[second]) {
                second = static_cast<int>(i);
            }
        }
        if (first == -1 || second == -1) {
            throw std::logic_error("Huffman tree construction ran out of nodes.");
        }
        used[first] = true;
        used[second] = true;
        weight[node_count] = weight[first] + weight[second];
        parent[first] = static_cast<int>(node_count);
        parent[second] = static_cast<int>(node_count);
        ++node_count;
    }

    // Depth of every leaf = its code length.
    for (std::size_t i = 0; i < leaves; ++i) {
        int depth = 0;
        int node = static_cast<int>(i);
        while (parent[node] != -1) {
            node = parent[node];
            ++depth;
        }
        lengths[symbol_of[i]] = static_cast<std::uint8_t>(depth);
    }
    return lengths;
}

// Canonical code assignment: symbols sorted by (length, symbol value) get
// consecutive codes, each code one longer where the length increases.
std::vector<std::uint32_t> build_codes(const std::array<std::uint8_t, 256>& lengths) {
    std::vector<std::uint32_t> codes(256, 0);
    std::uint32_t code = 0;
    for (int len = 1; len <= 255; ++len) {
        for (int s = 0; s < 256; ++s) {
            if (lengths[s] == static_cast<std::uint8_t>(len)) {
                codes[s] = code;
                ++code;
            }
        }
        code <<= 1;
    }
    return codes;
}

class BitWriter {
 public:
    void write(std::uint32_t code, int length) {
        for (int i = length - 1; i >= 0; --i) {
            const int bit = static_cast<int>((code >> i) & 1u);
            if (bit) bits_.push_back(1);
            else bits_.push_back(0);
        }
    }

    std::vector<std::uint8_t> bytes() const {
        std::vector<std::uint8_t> out((bits_.size() + 7) / 8, 0);
        for (std::size_t i = 0; i < bits_.size(); ++i) {
            if (bits_[i]) out[i / 8] |= static_cast<std::uint8_t>(1u << (7 - i % 8));
        }
        return out;
    }

 private:
    std::vector<std::uint8_t> bits_;
};

class BitReader {
 public:
    BitReader(const std::uint8_t* data, std::size_t n) : data_(data), size_(n) {}

    int read() {
        if (pos_ >= size_ * 8) return -1;
        const int bit = (data_[pos_ / 8] >> (7 - pos_ % 8)) & 1;
        ++pos_;
        return bit;
    }

 private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_ = 0;
};

}  // namespace

CodecResult HuffmanCoder::encode(ByteSpan in) const {
    CodecResult result;
    result.codec_id = id();
    result.original_size = in.size;

    std::array<std::uint64_t, 256> freq{};
    std::size_t distinct = 0;
    for (std::size_t i = 0; i < in.size; ++i) {
        if (freq[in[i]]++ == 0) ++distinct;
    }

    const auto lengths = build_lengths(freq, distinct);
    const auto codes = build_codes(lengths);

    result.data.reserve(kHeaderSize + in.size);
    result.data.resize(kTableSize);
    for (int s = 0; s < 256; ++s) result.data[s] = lengths[s];

    for (int i = 0; i < 8; ++i) {
        result.data.push_back(
            static_cast<std::uint8_t>(static_cast<std::uint64_t>(in.size) >> (8 * i)));
    }

    BitWriter writer;
    for (std::size_t i = 0; i < in.size; ++i) {
        writer.write(codes[in[i]], lengths[in[i]]);
    }
    auto bits = writer.bytes();
    result.data.insert(result.data.end(), bits.begin(), bits.end());

    result.is_valid = true;
    return result;
}

ByteBuffer HuffmanCoder::decode(ByteSpan in, std::size_t orig_size) const {
    ByteBuffer out;
    if (orig_size == 0) return out;
    if (in.size < kHeaderSize) {
        throw std::runtime_error("Huffman payload is truncated.");
    }

    std::array<std::uint8_t, 256> lengths{};
    for (int s = 0; s < 256; ++s) lengths[s] = in[s];

    std::uint64_t stored_size = 0;
    for (int i = 0; i < 8; ++i) {
        stored_size |= static_cast<std::uint64_t>(in[kTableSize + i]) << (8 * i);
    }
    if (stored_size != orig_size) {
        throw std::runtime_error("Huffman payload size disagrees with the block record.");
    }

    // Canonical decode: walk bits, matching against the canonical codes
    // rebuilt from the lengths. Codes are compared bit by bit, so no table
    // of 2^max_len entries is needed.
    const auto codes = build_codes(lengths);
    BitReader reader(in.data + kHeaderSize, in.size - kHeaderSize);

    out.reserve(orig_size);
    std::uint32_t code = 0;
    int length = 0;
    while (out.size() < orig_size) {
        const int bit = reader.read();
        if (bit == -1) {
            throw std::runtime_error("Huffman payload ran out of bits.");
        }
        code = (code << 1) | static_cast<std::uint32_t>(bit);
        ++length;

        bool matched = false;
        for (int s = 0; s < 256; ++s) {
            if (lengths[s] == static_cast<std::uint8_t>(length) && codes[s] == code) {
                out.push_back(static_cast<std::uint8_t>(s));
                code = 0;
                length = 0;
                matched = true;
                break;
            }
        }
        if (!matched && length > 255) {
            throw std::runtime_error("Huffman code longer than any stored length.");
        }
    }
    return out;
}

}  // namespace uatc::core
