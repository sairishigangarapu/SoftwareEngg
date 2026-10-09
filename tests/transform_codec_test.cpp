#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "archive/archive_reader.hpp"
#include "archive/archive_writer.hpp"
#include "archive/block.hpp"
#include "archive/codec.hpp"
#include "archive/transforms.hpp"

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    using namespace uatc;
    const std::vector<std::uint8_t> original = {
        0,0,0,0,0,1,2,3,4,4,4,4,4,9,8,7,6,5,5,5,5,5,5
    };
    const auto input = std::filesystem::path("uatc_transform_codec_input.bin");
    const auto archive = std::filesystem::path("uatc_transform_codec_test.uatc");

    try {
        {
            std::ofstream out(input, std::ios::binary);
            out.write(reinterpret_cast<const char*>(original.data()),
                      static_cast<std::streamsize>(original.size()));
        }

        for (auto transform : {TransformId::None, TransformId::RLE, TransformId::Delta}) {
            const auto transformed = Transform::encode(original, transform);
            require(Transform::decode(transformed, transform) == original,
                    "Transform round-trip failed.");
        }

        for (auto codec : {CodecId::None, CodecId::Baseline}) {
            const auto encoded = Codec::encode(original, codec);
            require(Codec::decode(encoded, codec) == original,
                    "Codec round-trip failed.");
        }

        const auto blocks = BlockSplitter::split(original, 7);
        for (auto transform : {TransformId::None, TransformId::RLE, TransformId::Delta}) {
            for (auto codec : {CodecId::None, CodecId::Baseline}) {
                ArchiveWriter::write(archive, input, blocks, transform, codec);
                require(ArchiveReader::read(archive) == original,
                        "Archive transform/codec round-trip failed.");
            }
        }

        bool rejected = false;
        try { Transform::decode({0, 42}, TransformId::RLE); }
        catch (const std::exception&) { rejected = true; }
        require(rejected, "Invalid RLE run was not rejected.");

        std::filesystem::remove(input);
        std::filesystem::remove(archive);
        std::cout << "ALL TRANSFORM/CODEC TESTS PASSED\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "TEST FAILED: " << e.what() << '\n';
        std::filesystem::remove(input);
        std::filesystem::remove(archive);
        return 1;
    }
}
