#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "archive/block.hpp"

int main() {
    std::cout << "UATC block splitter test started."
              << std::endl;

    try {
        // ----------------------------------------------------
        // TEST 1: Empty input
        // ----------------------------------------------------

        {
            std::vector<std::uint8_t> data;

            const auto blocks =
                uatc::BlockSplitter::split(data, 4);

            if (!blocks.empty()) {
                std::cerr
                    << "TEST FAILED: Empty input should produce 0 blocks."
                    << std::endl;
                return 1;
            }

            std::cout
                << "TEST PASSED: Empty input."
                << std::endl;
        }

        // ----------------------------------------------------
        // TEST 2: Data smaller than block size
        // ----------------------------------------------------

        {
            std::vector<std::uint8_t> data = {
                1, 2, 3
            };

            const auto blocks =
                uatc::BlockSplitter::split(data, 4);

            if (blocks.size() != 1) {
                std::cerr
                    << "TEST FAILED: Expected 1 block."
                    << std::endl;
                return 1;
            }

            if (blocks[0].data != data) {
                std::cerr
                    << "TEST FAILED: Block data is incorrect."
                    << std::endl;
                return 1;
            }

            if (blocks[0].block_id != 0 ||
                blocks[0].offset != 0) {
                std::cerr
                    << "TEST FAILED: Block metadata is incorrect."
                    << std::endl;
                return 1;
            }

            std::cout
                << "TEST PASSED: Data smaller than block size."
                << std::endl;
        }

        // ----------------------------------------------------
        // TEST 3: Exact block size
        // ----------------------------------------------------

        {
            std::vector<std::uint8_t> data = {
                1, 2, 3, 4
            };

            const auto blocks =
                uatc::BlockSplitter::split(data, 4);

            if (blocks.size() != 1) {
                std::cerr
                    << "TEST FAILED: Exact block size should produce 1 block."
                    << std::endl;
                return 1;
            }

            if (blocks[0].data != data) {
                std::cerr
                    << "TEST FAILED: Exact-size block data is incorrect."
                    << std::endl;
                return 1;
            }

            std::cout
                << "TEST PASSED: Exact block size."
                << std::endl;
        }

        // ----------------------------------------------------
        // TEST 4: Multiple complete blocks
        // ----------------------------------------------------

        {
            std::vector<std::uint8_t> data = {
                1, 2, 3, 4,
                5, 6, 7, 8,
                9, 10, 11, 12
            };

            const auto blocks =
                uatc::BlockSplitter::split(data, 4);

            if (blocks.size() != 3) {
                std::cerr
                    << "TEST FAILED: Expected 3 blocks."
                    << std::endl;
                return 1;
            }

            for (std::size_t i = 0; i < blocks.size(); ++i) {
                if (blocks[i].block_id != i) {
                    std::cerr
                        << "TEST FAILED: Incorrect block ID."
                        << std::endl;
                    return 1;
                }

                if (blocks[i].offset != i * 4) {
                    std::cerr
                        << "TEST FAILED: Incorrect block offset."
                        << std::endl;
                    return 1;
                }

                if (blocks[i].size() != 4) {
                    std::cerr
                        << "TEST FAILED: Incorrect block size."
                        << std::endl;
                    return 1;
                }
            }

            std::cout
                << "TEST PASSED: Multiple complete blocks."
                << std::endl;
        }

        // ----------------------------------------------------
        // TEST 5: Final partial block
        // ----------------------------------------------------

        {
            std::vector<std::uint8_t> data = {
                1, 2, 3, 4,
                5, 6, 7, 8,
                9, 10
            };

            const auto blocks =
                uatc::BlockSplitter::split(data, 4);

            if (blocks.size() != 3) {
                std::cerr
                    << "TEST FAILED: Expected 3 blocks."
                    << std::endl;
                return 1;
            }

            if (blocks[2].data !=
                std::vector<std::uint8_t>{9, 10}) {
                std::cerr
                    << "TEST FAILED: Final partial block is incorrect."
                    << std::endl;
                return 1;
            }

            if (blocks[2].offset != 8) {
                std::cerr
                    << "TEST FAILED: Final block offset is incorrect."
                    << std::endl;
                return 1;
            }

            std::cout
                << "TEST PASSED: Final partial block."
                << std::endl;
        }

        // ----------------------------------------------------
        // TEST 6: Invalid block size
        // ----------------------------------------------------

        {
            bool exception_thrown = false;

            try {
                std::vector<std::uint8_t> data = {
                    1, 2, 3
                };

                uatc::BlockSplitter::split(data, 0);
            }
            catch (const std::invalid_argument&) {
                exception_thrown = true;
            }

            if (!exception_thrown) {
                std::cerr
                    << "TEST FAILED: Block size 0 should throw."
                    << std::endl;
                return 1;
            }

            std::cout
                << "TEST PASSED: Invalid block size."
                << std::endl;
        }

        std::cout
            << "\nALL BLOCK SPLITTER TESTS PASSED!"
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