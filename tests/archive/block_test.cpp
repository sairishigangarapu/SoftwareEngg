
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "archive/block.hpp"

int main() {
    using uatc::BlockSplitter;
    using Byte = std::uint8_t;

    try {
        // Test 1: Empty input
        {
            const std::vector<Byte> data;
            const auto blocks = BlockSplitter::split(data, 4);

            if (!blocks.empty()) {
                throw std::runtime_error("Empty input should produce zero blocks.");
            }
        }

        // Test 2: Input smaller than block size
        {
            const std::vector<Byte> data{1, 2, 3};
            const auto blocks = BlockSplitter::split(data, 4);

            if (blocks.size() != 1 ||
                blocks[0].data != data ||
                blocks[0].block_id != 0 ||
                blocks[0].offset != 0) {
                throw std::runtime_error("Small-input test failed.");
            }
        }

        // Test 3: Exact block size
        {
            const std::vector<Byte> data{1, 2, 3, 4};
            const auto blocks = BlockSplitter::split(data, 4);

            if (blocks.size() != 1 || blocks[0].data != data) {
                throw std::runtime_error("Exact-size test failed.");
            }
        }

        // Test 4: Multiple complete blocks
        {
            const std::vector<Byte> data{
                1, 2, 3, 4,
                5, 6, 7, 8,
                9, 10, 11, 12
            };

            const auto blocks = BlockSplitter::split(data, 4);

            if (blocks.size() != 3) {
                throw std::runtime_error("Expected three blocks.");
            }

            for (std::size_t i = 0; i < blocks.size(); ++i) {
                if (blocks[i].block_id != i ||
                    blocks[i].offset != i * 4 ||
                    blocks[i].size() != 4) {
                    throw std::runtime_error("Block metadata is incorrect.");
                }
            }
        }

        // Test 5: Final partial block
        {
            const std::vector<Byte> data{
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10
            };

            const auto blocks = BlockSplitter::split(data, 4);

            if (blocks.size() != 3 ||
                blocks[2].data != std::vector<Byte>{9, 10} ||
                blocks[2].offset != 8) {
                throw std::runtime_error("Partial-block test failed.");
            }
        }

        // Test 6: Zero block size must throw
        {
            bool exception_thrown = false;

            try {
                const std::vector<Byte> data{1, 2, 3};
                BlockSplitter::split(data, 0);
            } catch (const std::invalid_argument&) {
                exception_thrown = true;
            }

            if (!exception_thrown) {
                throw std::runtime_error("Zero block size should throw.");
            }
        }

        std::cout << "ALL BLOCK SPLITTER TESTS PASSED!\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILED: " << error.what() << '\n';
        return 1;
    }
}
