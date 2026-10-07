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
        // ----------------------------------------------------
        // TEST 1: Write binary data
        // ----------------------------------------------------

        {
            uatc::BinaryWriter writer(test_file);

            writer.write_u8(0xAB);
            writer.write_u16(0x1234);
            writer.write_u32(0x12345678);
            writer.write_u64(0x1122334455667788ULL);

            std::vector<std::uint8_t> bytes = {
                0,
                1,
                2,
                3,
                4,
                5,
                255
            };

            writer.write_bytes(bytes);

            writer.write_string("UATC");

            std::cout
                << "TEST PASSED: BinaryWriter wrote test data."
                << std::endl;
        }

        // ----------------------------------------------------
        // TEST 2: Read binary data
        // ----------------------------------------------------

        {
            uatc::BinaryReader reader(test_file);

            const auto value8 = reader.read_u8();
            const auto value16 = reader.read_u16();
            const auto value32 = reader.read_u32();
            const auto value64 = reader.read_u64();

            if (value8 != 0xAB) {
                std::cerr
                    << "TEST FAILED: u8 value is incorrect."
                    << std::endl;
                return 1;
            }

            if (value16 != 0x1234) {
                std::cerr
                    << "TEST FAILED: u16 value is incorrect."
                    << std::endl;
                return 1;
            }

            if (value32 != 0x12345678) {
                std::cerr
                    << "TEST FAILED: u32 value is incorrect."
                    << std::endl;
                return 1;
            }

            if (value64 != 0x1122334455667788ULL) {
                std::cerr
                    << "TEST FAILED: u64 value is incorrect."
                    << std::endl;
                return 1;
            }

            std::cout
                << "TEST PASSED: Integer values were read correctly."
                << std::endl;

            // ------------------------------------------------
            // TEST 3: Read raw bytes
            // ------------------------------------------------

            const std::vector<std::uint8_t> expected_bytes = {
                0,
                1,
                2,
                3,
                4,
                5,
                255
            };

            const auto actual_bytes =
                reader.read_bytes(expected_bytes.size());

            if (actual_bytes != expected_bytes) {
                std::cerr
                    << "TEST FAILED: Raw bytes are incorrect."
                    << std::endl;
                return 1;
            }

            std::cout
                << "TEST PASSED: Raw bytes were read correctly."
                << std::endl;

            // ------------------------------------------------
            // TEST 4: Read string
            // ------------------------------------------------

            const auto text = reader.read_string(4);

            if (text != "UATC") {
                std::cerr
                    << "TEST FAILED: String value is incorrect."
                    << std::endl;
                return 1;
            }

            std::cout
                << "TEST PASSED: String was read correctly."
                << std::endl;
        }

        // ----------------------------------------------------
        // TEST 5: Confirm file exists
        // ----------------------------------------------------

        if (!std::filesystem::exists(test_file)) {
            std::cerr
                << "TEST FAILED: Test archive file was not created."
                << std::endl;
            return 1;
        }

        std::cout
            << "TEST PASSED: Archive binary file exists."
            << std::endl;

        // ----------------------------------------------------
        // Final result
        // ----------------------------------------------------

        std::cout
            << "\nALL ARCHIVE BINARY TESTS PASSED!"
            << std::endl;

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr
            << "\nTEST FAILED: "
            << e.what()
            << std::endl;

        return 1;
    }
}