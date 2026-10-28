#include "core/sha256.hpp"

#include <cstring>
#include <fstream>
#include <stdexcept>

#include "core/error.hpp"

namespace uatc::core {
namespace {

constexpr std::uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

inline std::uint32_t rotr(std::uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

// Processes one 512-bit block. `block` must be exactly 64 bytes.
void compress_block(Sha256State& state, const std::uint8_t* block) {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (std::uint32_t(block[i * 4 + 0]) << 24) |
               (std::uint32_t(block[i * 4 + 1]) << 16) |
               (std::uint32_t(block[i * 4 + 2]) << 8) |
                std::uint32_t(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = state.h[0], b = state.h[1], c = state.h[2], d = state.h[3];
    std::uint32_t e = state.h[4], f = state.h[5], g = state.h[6], h = state.h[7];

    for (int i = 0; i < 64; ++i) {
        const std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = h + S1 + ch + kK[i] + w[i];
        const std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = S0 + maj;

        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    state.h[0] += a; state.h[1] += b; state.h[2] += c; state.h[3] += d;
    state.h[4] += e; state.h[5] += f; state.h[6] += g; state.h[7] += h;
}

}  // namespace

Sha256State sha256_begin() {
    Sha256State state;
    state.h[0] = 0x6a09e667; state.h[1] = 0xbb67ae85;
    state.h[2] = 0x3c6ef372; state.h[3] = 0xa54ff53a;
    state.h[4] = 0x510e527f; state.h[5] = 0x9b05688c;
    state.h[6] = 0x1f83d9ab; state.h[7] = 0x5be0cd19;
    state.total_bits = 0;
    state.buffered = 0;
    return state;
}

void sha256_feed(Sha256State& state, const std::uint8_t* data, std::size_t n) {
    state.total_bits += static_cast<std::uint64_t>(n) * 8;

    // Fill the partial block first.
    if (state.buffered > 0) {
        const std::size_t take = (64 - state.buffered < n) ? 64 - state.buffered : n;
        std::memcpy(state.buffer + state.buffered, data, take);
        state.buffered += take;
        data += take;
        n -= take;
        if (state.buffered == 64) {
            compress_block(state, state.buffer);
            state.buffered = 0;
        }
    }

    // Whole blocks straight from the caller's memory.
    while (n >= 64) {
        compress_block(state, data);
        data += 64;
        n -= 64;
    }

    // Keep the remainder for the next call or for padding.
    if (n > 0) {
        std::memcpy(state.buffer + state.buffered, data, n);
        state.buffered += n;
    }
}

Sha256 sha256_end(const Sha256State& state) {
    Sha256State s = state;

    // Padding: a 0x80 byte, then zeros, then the 64-bit big-endian bit
    // length, so that the total is a whole number of 512-bit blocks.
    std::uint8_t padding[72] = {};
    padding[0] = 0x80;
    const std::uint64_t bits = s.total_bits;

    std::size_t pad_len = (s.buffered < 56) ? 56 - s.buffered : 120 - s.buffered;
    sha256_feed(s, padding, pad_len);

    std::uint8_t length[8];
    for (int i = 0; i < 8; ++i) length[i] = static_cast<std::uint8_t>(bits >> (56 - i * 8));
    sha256_feed(s, length, 8);

    if (s.buffered != 0) {
        throw std::logic_error("SHA-256 finalisation left an unfinished block");
    }

    Sha256 digest;
    for (int i = 0; i < 8; ++i) {
        digest[i * 4 + 0] = static_cast<std::uint8_t>(s.h[i] >> 24);
        digest[i * 4 + 1] = static_cast<std::uint8_t>(s.h[i] >> 16);
        digest[i * 4 + 2] = static_cast<std::uint8_t>(s.h[i] >> 8);
        digest[i * 4 + 3] = static_cast<std::uint8_t>(s.h[i]);
    }
    return digest;
}

Sha256 sha256(const std::uint8_t* data, std::size_t n) {
    auto state = sha256_begin();
    sha256_feed(state, data, n);
    return sha256_end(state);
}

std::string sha256_hex(const Sha256& digest) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(kSha256Size * 2);
    for (std::uint8_t byte : digest) {
        out.push_back(digits[byte >> 4]);
        out.push_back(digits[byte & 0x0F]);
    }
    return out;
}

Sha256 sha256_of_file_prefix(const std::filesystem::path& path,
                             std::uint64_t n) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw UatcError(ErrorCode::Io,
                        "Cannot read: " + path.string());
    }

    auto state = sha256_begin();
    std::vector<std::uint8_t> buffer(
        1 << 16);
    std::uint64_t remaining = n;
    while (remaining > 0) {
        const std::size_t take = static_cast<std::size_t>(
            remaining < buffer.size() ? remaining : buffer.size());
        in.read(reinterpret_cast<char*>(buffer.data()),
                static_cast<std::streamsize>(take));
        if (!in && !in.eof()) {
            throw UatcError(ErrorCode::Io,
                            "Read failed: " + path.string());
        }
        const auto got = static_cast<std::size_t>(in.gcount());
        if (got == 0) break;
        sha256_feed(state, buffer.data(), got);
        remaining -= got;
    }
    if (remaining != 0) {
        throw UatcError(ErrorCode::MalformedArchive,
                        "The archive is shorter than its own integrity "
                        "region: " + path.string());
    }
    return sha256_end(state);
}

}  // namespace uatc::core
