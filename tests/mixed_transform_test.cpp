
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "archive/archive_reader.hpp"
#include "archive/archive_writer.hpp"
#include "archive/block.hpp"
#include "archive/transforms.hpp"

int main() {
    namespace fs = std::filesystem;
    using namespace uatc;

    const fs::path source = "mixed_transform_input.bin";
    const fs::path archive = "mixed_transform_test.uatc";

    try {
        const std::vector<std::uint8_t> original = {
            10, 20, 30, 40,
            11, 21, 31, 41,
            12, 22, 32, 42
        };

        {
            std::ofstream out(source, std::ios::binary);
            out.write(
                reinterpret_cast<const char*>(original.data()),
                static_cast<std::streamsize>(original.size())
            );

            if (!out) {
                throw std::runtime_error("Could not write input.");
            }
        }

        const auto blocks = BlockSplitter::split(original, 4);

        if (blocks.size() != 3) {
            throw std::runtime_error("Expected exactly three blocks.");
        }

        const std::vector<BlockEncoding> encodings = {
            {TransformId::None, CodecId::None},
            {TransformId::Delta, CodecId::None},
            {TransformId::None, CodecId::None}
        };

        ArchiveWriter::write(archive, source, blocks, encodings);

        const auto extracted = ArchiveReader::read(archive);

        if (extracted != original) {
            throw std::runtime_error(
                "Mixed-transform archive did not reconstruct original bytes."
            );
        }

        std::error_code ec;
        fs::remove(source, ec);
        fs::remove(archive, ec);

        std::cout
            << "PASSED: Mixed None/Delta archive round-trip.\n";
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';

        std::error_code ec;
        fs::remove(source, ec);
        fs::remove(archive, ec);
        return 1;
    }
}
