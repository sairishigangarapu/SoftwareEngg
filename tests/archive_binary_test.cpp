
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "archive/archive_binary.hpp"

int main() {
    const std::filesystem::path test_file =
        "test_data/archive_binary_test.bin";

    std::cout << "UATC archive binary test started."
              << std::endl;

    try {
        // TEST 1: Write binary data
        {
            uatc::BinaryWriter writer(test_file);

            writer.write_u8(0xAB);
            writer.write_u16(0x1234);
            writer.write_u32(0x12345678);
            writer.write_u64(0x1122334455667788ULL);

            const std::vector<std::uint8_t> bytes = {
                0, 1, 2, 3, 4, 5, 255
            };

            writer.write_bytes(bytes);
            writer.write_string("UATC");

            std::cout
                << "TEST PASSED: BinaryWriter wrote test data."
                << std::endl;
        }

        // TEST 2: Read and verify binary data
        {
            uatc::BinaryReader reader(test_file);

            if (reader.read_u8() != 0xAB) {
                throw std::runtime_error("Incorrect u8 value.");
            }

            if (reader.read_u16() != 0x1234) {
                throw std::runtime_error("Incorrect u16 value.");
            }

            if (reader.read_u32() != 0x12345678) {
                throw std::runtime_error("Incorrect u32 value.");
            }

            if (reader.read_u64() != 0x1122334455667788ULL) {
                throw std::runtime_error("Incorrect u64 value.");
            }

            std::cout
                << "TEST PASSED: Integer values were read correctly."
                << std::endl;

            const std::vector<std::uint8_t> expected_bytes = {
                0, 1, 2, 3, 4, 5, 255
            };

            if (reader.read_bytes(expected_bytes.size())
                != expected_bytes) {
                throw std::runtime_error("Incorrect raw bytes.");
            }

            std::cout
                << "TEST PASSED: Raw bytes were read correctly."
                << std::endl;

            if (reader.read_string(4) != "UATC") {
                throw std::runtime_error("Incorrect string value.");
            }

            std::cout
                << "TEST PASSED: String was read correctly."
                << std::endl;
        }

        // TEST 3: Confirm the file exists
        if (!std::filesystem::exists(test_file)) {
            throw std::runtime_error(
                "Test archive file was not created."
            );
        }

        std::cout
            << "TEST PASSED: Archive binary file exists."
            << std::endl;

        std::cout
            << "\nALL ARCHIVE BINARY TESTS PASSED!"
            << std::endl;

        // Remove the generated test file
        std::filesystem::remove(test_file);

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\nTEST FAILED: " << e.what() << std::endl;

        // Clean up generated output even if a test fails
        std::error_code cleanup_error;
        std::filesystem::remove(test_file, cleanup_error);

        return 1;
    }
}
