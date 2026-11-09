#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/sha256.hpp"

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string hash_text(const std::string& value) {
    return uatc::core::sha256_hex(uatc::core::sha256(
        reinterpret_cast<const std::uint8_t*>(value.data()),
        value.size()
    ));
}

void check_standard_vectors() {
    expect(
        hash_text("") ==
            "e3b0c44298fc1c149afbf4c8996fb924"
            "27ae41e4649b934ca495991b7852b855",
        "SHA-256 empty vector failed."
    );
    expect(
        hash_text("abc") ==
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad",
        "SHA-256 abc vector failed."
    );
    expect(
        hash_text(
            "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
        ) ==
            "248d6a61d20638b8e5c026930c3e6039"
            "a33ce45964ff2167f6ecedd419db06c1",
        "SHA-256 multi-block vector failed."
    );
}

void check_incremental_and_padding_boundaries() {
    std::vector<std::uint8_t> data(200);
    for (std::size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<std::uint8_t>(i * 7);
    }

    for (std::size_t length = 0; length <= data.size(); ++length) {
        auto state = uatc::core::sha256_begin();
        std::size_t offset = 0;
        while (offset < length) {
            const std::size_t count = std::min<std::size_t>(
                7,
                length - offset
            );
            uatc::core::sha256_feed(state, data.data() + offset, count);
            offset += count;
        }
        expect(
            uatc::core::sha256_end(state) ==
                uatc::core::sha256(data.data(), length),
            "Incremental SHA-256 disagrees with one-shot hashing."
        );
    }

    std::vector<std::uint8_t> chunk(1000, 'a');
    auto million_a = uatc::core::sha256_begin();
    for (int i = 0; i < 1000; ++i) {
        uatc::core::sha256_feed(
            million_a,
            chunk.data(),
            chunk.size()
        );
    }
    expect(
        uatc::core::sha256_hex(uatc::core::sha256_end(million_a)) ==
            "cdc76e5c9914fb9281a1c7e284d73e67"
            "f1809a48a497200e046d39ccc7112cd0",
        "SHA-256 million-a vector failed."
    );
}

void check_file_prefix_hash() {
    namespace fs = std::filesystem;
    const auto unique = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    const auto path = fs::temp_directory_path() /
        ("uatc_sha256_test_" + std::to_string(unique));
    const std::string value = "abc";

    try {
        std::ofstream output(path, std::ios::binary);
        output.write(value.data(), static_cast<std::streamsize>(value.size()));
        output.close();
        if (!output) {
            throw std::runtime_error("Could not write SHA-256 file fixture.");
        }

        expect(
            uatc::core::sha256_of_file_prefix(path, value.size()) ==
                uatc::core::sha256(
                    reinterpret_cast<const std::uint8_t*>(value.data()),
                    value.size()
                ),
            "SHA-256 file-prefix digest failed."
        );
        bool rejected_short_prefix = false;
        try {
            static_cast<void>(
                uatc::core::sha256_of_file_prefix(path, value.size() + 1)
            );
        } catch (const std::runtime_error&) {
            rejected_short_prefix = true;
        }
        expect(rejected_short_prefix, "Short file prefix was not rejected.");
    } catch (...) {
        std::error_code ignored;
        fs::remove(path, ignored);
        throw;
    }
    fs::remove(path);
}

}  // namespace

int main() {
    check_standard_vectors();
    check_incremental_and_padding_boundaries();
    check_file_prefix_hash();
}
