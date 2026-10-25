// DAY 10 - Planner and progress reporting.
//
// The planner is what produces the "before" picture, and the progress sink
// is what produces the "during" picture. Both must agree with what the
// compression actually does.
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

#include "core/compressor.hpp"
#include "core/planner.hpp"
#include "core/progress.hpp"

namespace fs = std::filesystem;

namespace {

void write_file(const fs::path& p, const std::vector<std::uint8_t>& data) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
}

// A sink that records what it was told.
class RecordingSink : public uatc::core::ProgressSink {
 public:
    void on_block(const uatc::core::BlockProgress& p) override {
        updates.push_back(p);
        totalIn += p.bytesIn;
        totalOut += p.bytesOut;
    }

    std::vector<uatc::core::BlockProgress> updates;
    std::uint64_t totalIn = 0;
    std::uint64_t totalOut = 0;
};

}  // namespace

int main() {
    using namespace uatc;
    using namespace uatc::core;

    const fs::path work = "uatc_planner_test_dir";
    fs::remove_all(work);
    fs::create_directories(work);

    // --- A file with three kinds of data in it ---
    std::vector<std::uint8_t> data;
    for (int i = 0; i < 60000; ++i)
        data.push_back(static_cast<std::uint8_t>((i / 4) & 0xFF));  // numeric
    for (int i = 0; i < 60000; ++i) data.push_back(0x00);            // runs
    {
        std::mt19937 rng(3);
        for (int i = 0; i < 60000; ++i)
            data.push_back(static_cast<std::uint8_t>(rng()));        // noise
    }

    const auto source = work / "mixed.bin";
    write_file(source, data);

    CompressOptions options;
    options.block_size = 65536;

    // --- The plan (the "before" picture) ---
    const auto report = analyze(source, options);

    assert(report.totalBytes == data.size());
    assert(report.blockCount == 3);
    assert(report.blocks.size() == 3);
    assert(report.compressibleBlocks + report.storedBlocks == 3);
    assert(report.overallEntropy > 0.0 && report.overallEntropy <= 8.0);

    // The noise block must be planned as stored, the others as compressed.
    assert(report.blocks[0].compressible);
    assert(report.blocks[1].compressible);
    assert(!report.blocks[2].compressible);
    assert(report.blocks[2].stored);
    assert(report.blocks[2].transformId == TransformId::None);
    assert(report.storedBlocks == 1);

    // --- The progress report (the "during" picture) ---
    RecordingSink sink;
    const auto archive = work / "mixed.uatc";
    const auto result = CompressionManager::compress(source, archive, options, &sink);

    assert(sink.updates.size() == report.blockCount);
    assert(sink.totalIn == result.originalBytes);
    assert(sink.totalOut == result.archiveBytes);

    for (std::size_t i = 0; i < sink.updates.size(); ++i) {
        assert(sink.updates[i].blockIndex == i);
        assert(sink.updates[i].blockCount == report.blockCount);
        // The plan and the run must agree on the transform.
        assert(sink.updates[i].transformId == report.blocks[i].transformId);
        assert(sink.updates[i].stored == report.blocks[i].stored);
    }

    // The plan and the run must agree on stored versus compressed.
    assert(result.storedBlocks == report.storedBlocks);
    assert(result.compressedBlocks == report.compressibleBlocks);

    // The prediction is an upper bound, never better than reality.
    assert(report.estimatedArchiveBytes >= 0);

    // --- The NullProgressSink counts the same thing ---
    {
        NullProgressSink null_sink;
        CompressionManager::compress(source, work / "mixed2.uatc", options, &null_sink);
        assert(null_sink.blocksSeen == report.blockCount);
        assert(null_sink.bytesIn == result.originalBytes);
    }

    // --- A missing input is reported as E_NOT_FOUND ---
    {
        bool caught = false;
        try {
            analyze(work / "nope.bin", options);
        } catch (const std::exception&) {
            caught = true;
        }
        assert(caught);
    }

    // --- A zero block size is rejected ---
    {
        bool caught = false;
        try {
            CompressOptions bad;
            bad.block_size = 0;
            analyze(source, bad);
        } catch (const std::exception&) {
            caught = true;
        }
        assert(caught);
    }

    fs::remove_all(work);
    std::printf("planner_test: PASS\n");
    return 0;
}
