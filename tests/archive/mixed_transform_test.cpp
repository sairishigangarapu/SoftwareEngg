
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
        // Four blocks of four bytes each.
        const std::vector<std::uint8_t> original = {
            10, 20, 30, 40,       // Raw
            50, 60, 70, 80,       // Codec only
            1, 2, 3, 4,           // Delta + codec
            7, 7, 7, 7            // RLE + codec
        };

        {
            std::ofstream out(source, std::ios::binary);
            if (!out) {
                throw std::runtime_error(
                    "Could not open input file."
                );
            }

            out.write(
                reinterpret_cast<const char*>(original.data()),
                static_cast<std::streamsize>(original.size())
            );

            if (!out) {
                throw std::runtime_error(
                    "Could not write input file."
                );
            }
        }

        const auto blocks = BlockSplitter::split(original, 4);

        if (blocks.size() != 4) {
            throw std::runtime_error(
                "Expected exactly four blocks."
            );
        }

        const std::vector<BlockEncoding> encodings = {
            {TransformId::None, CodecId::None},
            {TransformId::None, CodecId::Baseline},
            {TransformId::Delta, CodecId::Baseline},
            {TransformId::RLE, CodecId::Baseline}
        };

        ArchiveWriter::write(
            archive,
            source,
            blocks,
            encodings
        );

        const auto extracted = ArchiveReader::read(archive);

        if (extracted != original) {
            throw std::runtime_error(
                "Mixed-encoding archive did not reconstruct "
                "the original bytes."
            );
        }

        std::error_code ec;
        fs::remove(source, ec);
        fs::remove(archive, ec);

        std::cout
            << "PASSED: Mixed raw, codec, Delta and RLE blocks.\n";

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
