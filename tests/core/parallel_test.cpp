// DAY 14 - Parallel pipeline and determinism (FR-10) tests.
//
// TC-PAR-01: the same input compressed with 1 worker and with 8 workers
// must produce byte-identical archives. TC-PAR-05: small inputs use the
// serial path and do not regress.
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <thread>
#include <vector>

#include "archive/archive_reader.hpp"
#include "core/compressor.hpp"
#include "core/progress.hpp"

namespace fs = std::filesystem;

namespace {

void write_file(const fs::path& p, const std::vector<std::uint8_t>& data) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
}

std::vector<std::uint8_t> read_bytes_safe(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<std::uint8_t>(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
    using namespace uatc;
    using namespace uatc::core;

    const fs::path work = "uatc_parallel_test_dir";
    fs::remove_all(work);
    fs::create_directories(work);

    // Large enough (4 MiB) to cross the parallel threshold, with mixed
    // content so each block gets its own treatment.
    std::vector<std::uint8_t> data;
    for (int i = 0; i < 400000; ++i)
        data.push_back(static_cast<std::uint8_t>((i / 7) & 0xFF));
    for (int i = 0; i < 1000000; ++i) data.push_back(0x00);
    {
        std::mt19937 rng(17);
        for (int i = 0; i < 1000000; ++i)
            data.push_back(static_cast<std::uint8_t>(rng()));
    }
    for (int i = 0; i < 700000; ++i)
        data.push_back(static_cast<std::uint8_t>((i * i) & 0xFF));

    const auto source = work / "big.bin";
    write_file(source, data);

    CompressOptions options;
    options.block_size = 256 * 1024;  // 16 blocks

    // --- TC-PAR-01: 1 worker and 8 workers give byte-identical archives ---
    std::vector<std::uint8_t> one_worker_archive;
    for (std::size_t workers : {std::size_t{1}, std::size_t{2},
                                std::size_t{8}}) {
        options.workers = workers;
        const auto archive = work / ("par_" + std::to_string(workers) + ".uatc");

        const auto result = CompressionManager::compress(source, archive, options);
        assert(result.blockCount == 16);

        auto archive_bytes = read_bytes_safe(archive);
        if (workers == 1) {
            one_worker_archive = archive_bytes;
        } else {
            assert(archive_bytes == one_worker_archive);
        }
    }

    // --- Progress is reported in block order, whatever the worker count ---
    {
        class OrderSink : public ProgressSink {
         public:
            void on_block(const BlockProgress& p) override {
                assert(p.blockIndex == next++);
                order.push_back(p.blockIndex);
            }
            std::vector<std::uint64_t> order;
            std::uint64_t next = 0;
        } sink;

        options.workers = 8;
        CompressionManager::compress(source, work / "order.uatc", options, &sink);
        assert(sink.order.size() == 16);
        for (std::uint64_t i = 0; i < 16; ++i) assert(sink.order[i] == i);
    }

    // --- Every parallel archive still round-trips ---
    {
        options.workers = 8;
        const auto archive = work / "roundtrip.uatc";
        CompressionManager::compress(source, archive, options);

        auto restored = CompressionManager::extract(archive, work / "restored");
        assert(read_bytes_safe(restored.destination) == data);
    }

    // --- TC-PAR-05: a small input takes the serial path and still works ---
    {
        std::vector<std::uint8_t> small(10000);
        {
            std::mt19937 rng(3);
            for (auto& b : small) b = static_cast<std::uint8_t>(rng());
        }
        const auto small_source = work / "small.bin";
        write_file(small_source, small);

        options.workers = 8;
        const auto small_archive = work / "small.uatc";
        CompressionManager::compress(small_source, small_archive, options);

        auto restored = CompressionManager::extract(small_archive, work / "small_restored");
        assert(read_bytes_safe(restored.destination) == small);
    }

    fs::remove_all(work);
    std::printf("parallel_test: PASS\n");
    return 0;
}
