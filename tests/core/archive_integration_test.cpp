// DAY 9 - Integration: the compression core and the archive layer.
//
// Every transform in the v1 candidate set must survive a full archive
// round-trip, and the adaptive pipeline must produce archives that the
// archive layer can read back byte-for-byte.
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

#include "archive/archive_reader.hpp"
#include "archive/archive_writer.hpp"
#include "archive/block.hpp"
#include "core/transform_factory.hpp"
#include "core/transform_selector.hpp"
#include "core/compressibility_probe.hpp"

namespace fs = std::filesystem;

namespace {

void write_file(const std::filesystem::path& p, const std::vector<std::uint8_t>& data) {
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
}

}  // namespace

int main() {
    using namespace uatc;
    using namespace uatc::core;

    // --- Build a source file with mixed content ---
    std::vector<std::uint8_t> original;
    for (int i = 0; i < 300; ++i) original.push_back(static_cast<std::uint8_t>(i & 0xFF));
    for (int i = 0; i < 400; ++i) original.push_back(0x00);
    for (int i = 0; i < 300; ++i) original.push_back(static_cast<std::uint8_t>(i / 4));
    for (int i = 0; i < 200; ++i) original.push_back(0x5A);

    const fs::path source = "uatc_core_archive_integration.bin";
    const fs::path archive = "uatc_core_archive_integration.uatc";
    write_file(source, original);

    // --- Every transform survives a full archive round-trip ---
    for (TransformId id : candidate_transform_ids()) {
        const auto blocks = BlockSplitter::split(original, 128);
        ArchiveWriter::write(archive, source, blocks, id, CodecId::None);
        auto extracted = ArchiveReader::read(archive);
        assert(extracted == original);
        std::filesystem::remove(archive);
    }

    // --- Every transform survives with the baseline codec applied ---
    for (TransformId id : candidate_transform_ids()) {
        const auto blocks = BlockSplitter::split(original, 128);
        ArchiveWriter::write(archive, source, blocks, id, CodecId::Baseline);
        auto extracted = ArchiveReader::read(archive);
        assert(extracted == original);
        std::filesystem::remove(archive);
    }

    // --- Mixed per-block transforms in one archive ---
    {
        const auto blocks = BlockSplitter::split(original, 128);
        std::vector<BlockEncoding> encodings;
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            BlockEncoding e;
            e.transform_id = candidate_transform_ids()[i % 5];
            e.codec_id = (i % 2 == 0) ? CodecId::None : CodecId::Baseline;
            encodings.push_back(e);
        }
        ArchiveWriter::write(archive, source, blocks, encodings);
        auto extracted = ArchiveReader::read(archive);
        assert(extracted == original);
        std::filesystem::remove(archive);
    }

    // --- The adaptive pipeline produces an archive the layer can read ---
    {
        auto registry = make_default_registry();
        TransformSelector selector(registry);
        CompressibilityProbe probe;

        const auto blocks = BlockSplitter::split(original, 128);
        std::vector<BlockEncoding> encodings;
        for (const auto& block : blocks) {
            BlockEncoding e;
            if (probe.is_compressible(ByteSpan(block.data))) {
                auto selection = selector.select(ByteSpan(block.data));
                e.transform_id = selection.transform_id;
            } else {
                e.transform_id = TransformId::None;
            }
            e.codec_id = CodecId::Baseline;
            encodings.push_back(e);
        }

        ArchiveWriter::write(archive, source, blocks, encodings);
        auto extracted = ArchiveReader::read(archive);
        assert(extracted == original);
        std::filesystem::remove(archive);
    }

    // --- A corrupt payload is still detected (integrity unchanged) ---
    {
        const auto blocks = BlockSplitter::split(original, 128);
        ArchiveWriter::write(archive, source, blocks, TransformId::RLE, CodecId::None);

        std::fstream f(archive, std::ios::binary | std::ios::in | std::ios::out);
        f.seekp(-1, std::ios::end);
        char last = 0;
        f.read(&last, 1);
        f.seekp(-1, std::ios::end);
        last = static_cast<char>(last ^ 0xFF);
        f.write(&last, 1);
        f.close();

        bool rejected = false;
        try { ArchiveReader::read(archive); }
        catch (const std::exception&) { rejected = true; }
        assert(rejected);
        std::filesystem::remove(archive);
    }

    std::filesystem::remove(source);
    std::printf("archive_integration_test: PASS\n");
    return 0;
}
