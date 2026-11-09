#include "core/sha256.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace uatc::core {
namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants = {{
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
}};

std::uint32_t rotate_right(std::uint32_t value, unsigned int amount) {
    return (value >> amount) | (value << (32 - amount));
}

void process_block(Sha256State& state, const std::uint8_t* block) {
    std::uint32_t words[64];
    for (std::size_t i = 0; i < 16; ++i) {
        const std::size_t offset = i * 4;
        words[i] =
            (static_cast<std::uint32_t>(block[offset]) << 24) |
            (static_cast<std::uint32_t>(block[offset + 1]) << 16) |
            (static_cast<std::uint32_t>(block[offset + 2]) << 8) |
            static_cast<std::uint32_t>(block[offset + 3]);
    }

    for (std::size_t i = 16; i < 64; ++i) {
        const std::uint32_t s0 =
            rotate_right(words[i - 15], 7) ^
            rotate_right(words[i - 15], 18) ^
            (words[i - 15] >> 3);
        const std::uint32_t s1 =
            rotate_right(words[i - 2], 17) ^
            rotate_right(words[i - 2], 19) ^
            (words[i - 2] >> 10);
        words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }

    std::uint32_t a = state.h[0];
    std::uint32_t b = state.h[1];
    std::uint32_t c = state.h[2];
    std::uint32_t d = state.h[3];
    std::uint32_t e = state.h[4];
    std::uint32_t f = state.h[5];
    std::uint32_t g = state.h[6];
    std::uint32_t h = state.h[7];

    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint32_t sum1 =
            rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
        const std::uint32_t choose = (e & f) ^ (~e & g);
        const std::uint32_t temp1 =
            h + sum1 + choose + kRoundConstants[i] + words[i];
        const std::uint32_t sum0 =
            rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = sum0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state.h[0] += a;
    state.h[1] += b;
    state.h[2] += c;
    state.h[3] += d;
    state.h[4] += e;
    state.h[5] += f;
    state.h[6] += g;
    state.h[7] += h;
}

}  // namespace

Sha256State sha256_begin() {
    Sha256State state{};
    state.h[0] = 0x6a09e667u;
    state.h[1] = 0xbb67ae85u;
    state.h[2] = 0x3c6ef372u;
    state.h[3] = 0xa54ff53au;
    state.h[4] = 0x510e527fu;
    state.h[5] = 0x9b05688cu;
    state.h[6] = 0x1f83d9abu;
    state.h[7] = 0x5be0cd19u;
    return state;
}

void sha256_feed(
    Sha256State& state,
    const std::uint8_t* data,
    std::size_t size
) {
    if (size == 0) {
        return;
    }
    if (size >
        (std::numeric_limits<std::uint64_t>::max() - state.total_bits) / 8) {
        throw std::length_error("SHA-256 input exceeds the supported length.");
    }
    state.total_bits += static_cast<std::uint64_t>(size) * 8;

    if (state.buffered != 0) {
        const std::size_t take = std::min(64 - state.buffered, size);
        std::copy(data, data + take, state.buffer + state.buffered);
        state.buffered += take;
        data += take;
        size -= take;
        if (state.buffered == 64) {
            process_block(state, state.buffer);
            state.buffered = 0;
        }
    }

    while (size >= 64) {
        process_block(state, data);
        data += 64;
        size -= 64;
    }

    if (size != 0) {
        std::copy(data, data + size, state.buffer + state.buffered);
        state.buffered += size;
    }
}

Sha256 sha256_end(const Sha256State& initial_state) {
    Sha256State state = initial_state;
    const std::uint64_t message_bits = state.total_bits;
    std::uint8_t padding[64] = {0x80};
    const std::size_t padding_size =
        state.buffered < 56 ? 56 - state.buffered : 120 - state.buffered;
    state.total_bits = 0;
    sha256_feed(state, padding, padding_size);

    std::uint8_t length[8];
    for (std::size_t i = 0; i < 8; ++i) {
        length[i] = static_cast<std::uint8_t>(
            message_bits >> (56 - i * 8)
        );
    }
    sha256_feed(state, length, sizeof(length));

    if (state.buffered != 0) {
        throw std::logic_error("SHA-256 finalization left a partial block.");
    }

    Sha256 digest{};
    for (std::size_t i = 0; i < 8; ++i) {
        digest[i * 4] = static_cast<std::uint8_t>(state.h[i] >> 24);
        digest[i * 4 + 1] = static_cast<std::uint8_t>(state.h[i] >> 16);
        digest[i * 4 + 2] = static_cast<std::uint8_t>(state.h[i] >> 8);
        digest[i * 4 + 3] = static_cast<std::uint8_t>(state.h[i]);
    }
    return digest;
}

Sha256 sha256(const std::uint8_t* data, std::size_t size) {
    auto state = sha256_begin();
    sha256_feed(state, data, size);
    return sha256_end(state);
}

std::string sha256_hex(const Sha256& digest) {
    constexpr char digits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(kSha256Size * 2);
    for (const std::uint8_t byte : digest) {
        hex.push_back(digits[byte >> 4]);
        hex.push_back(digits[byte & 0x0fu]);
    }
    return hex;
}

Sha256 sha256_of_file_prefix(
    const std::filesystem::path& path,
    std::uint64_t size
) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not read file for SHA-256: " + path.string());
    }

    auto state = sha256_begin();
    std::array<std::uint8_t, 64 * 1024> buffer{};
    std::uint64_t remaining = size;
    while (remaining != 0) {
        const std::size_t requested = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining, buffer.size())
        );
        input.read(
            reinterpret_cast<char*>(buffer.data()),
            static_cast<std::streamsize>(requested)
        );
        const auto count = static_cast<std::size_t>(input.gcount());
        if (count != requested) {
            throw std::runtime_error(
                "Archive is truncated while computing its SHA-256."
            );
        }
        sha256_feed(state, buffer.data(), count);
        remaining -= count;
    }
    return sha256_end(state);
}

}  // namespace uatc::core
