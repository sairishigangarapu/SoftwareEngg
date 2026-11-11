
#include "archive/archive_inspector.hpp"
#include "archive/archive_reader.hpp"
#include "archive/archive_verifier.hpp"
#include "archive/archive_writer.hpp"
#include "archive/block.hpp"
#include "archive/multi_file.hpp"
#include "core/compressibility_probe.hpp"
#include "core/compressor.hpp"
#include "core/config.hpp"
#include "core/error.hpp"
#include "core/path_safety.hpp"
#include "core/planner.hpp"
#include "core/progress.hpp"
#include "core/transform_factory.hpp"
#include "core/transform_selector.hpp"
#include "core/thread_pool.hpp"
#include "filesystem/file_enumerator.hpp"
#include "filesystem/file_reader.hpp"

#include "server/api_server.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

// The name comes from the compression core, so the CLI always agrees with the
// transform engine and needs no change when a transform is added.
const char* transform_name(uatc::TransformId id) {
    static thread_local std::string name;
    auto transform = uatc::core::make_transform(id);
    name = transform ? transform->name() : "Unknown";
    return name.c_str();
}

// A fixed-width label for a transform, for the aligned progress and stats
// tables.
const char* transform_label(uatc::TransformId id) {
    switch (id) {
        case uatc::TransformId::None:
            return "None";
        case uatc::TransformId::Delta:
            return "Delta";
        case uatc::TransformId::Xor:
            return "XOR";
        case uatc::TransformId::ByteShuffle:
            return "ByteShuffle";
        case uatc::TransformId::RLE:
            return "RLE";
        default:
            return "Unknown";
    }
}

// Reads a whole file, for the round-trip comparison in the benchmark.
std::vector<std::uint8_t> read_file_bytes(const std::filesystem::path& path);

// DAY 23 - Directory compression: walk the tree, encode every file with the
// adaptive engine, and write one multi-file archive (FR-01/02).
int compress_directory(const std::filesystem::path& dir,
                       const std::filesystem::path& output,
                       std::size_t block_size,
                       std::size_t workers,
                       bool quiet) {
    const auto files = uatc::FileEnumerator::enumerate(dir);
    if (files.empty()) {
        std::cerr << "error: E_INVALID_ARGUMENT: the directory holds no "
                     "regular files: " << dir.string() << '\n';
        return 2;
    }

    const auto registry = uatc::core::make_default_registry();
    const uatc::core::TransformSelector selector(registry);
    const uatc::core::CompressibilityProbe probe;

    const std::size_t file_workers =
        (workers > 1 && files.size() > 1) ? workers : 1;

    std::vector<uatc::ArchiveInput> inputs;
    std::vector<std::vector<uatc::CodedBlock>> coded;
    inputs.reserve(files.size());
    coded.reserve(files.size());

    std::uint64_t total_original = 0;
    std::map<uatc::TransformId, std::uint64_t> transform_usage;

    // Encode files in deterministic order; the per-file block pipeline can
    // still use the pool internally when there is more than one file.
    for (std::size_t f = 0; f < files.size(); ++f) {
        const auto path = dir / files[f].relative_path.generic_string();
        const auto data = uatc::FileReader::read(path);
        total_original += data.size();

        uatc::ArchiveInput input;
        input.relative_path = files[f].relative_path.generic_string();
        input.data = data;

        const auto blocks =
            uatc::BlockSplitter::split(data, block_size);
        std::vector<uatc::CodedBlock> per_file;
        per_file.reserve(blocks.size());

        if (file_workers > 1 && blocks.size() > 1) {
            auto pool = uatc::core::make_thread_pool(file_workers, file_workers * 4);
            std::vector<uatc::CodedBlock> out(blocks.size());
            for (std::size_t b = 0; b < blocks.size(); ++b) {
                pool->submit([&blocks, &selector, &probe, &out, b] {
                    out[b] = uatc::core::CompressionManager::encode_block(
                        blocks[b], selector, probe);
                });
            }
            pool->wait_idle();
            per_file = std::move(out);
        } else {
            for (const auto& block : blocks) {
                per_file.push_back(
                    uatc::core::CompressionManager::encode_block(
                        block, selector, probe));
            }
        }

        for (const auto& entry : per_file) {
            ++transform_usage[entry.transform_id];
        }
        coded.push_back(std::move(per_file));
        inputs.push_back(std::move(input));
    }

    uatc::MultiFileArchiveWriter::write(output, inputs, coded, block_size);

    if (quiet) {
        return 0;
    }

    const auto archive_bytes = std::filesystem::file_size(output);

    std::cout
        << "Compressed:        " << dir.string() << " (directory)\n"
        << "Archive:           " << output.string() << '\n'
        << "Files:             " << files.size() << '\n'
        << "Original bytes:    " << total_original << '\n'
        << "Archive bytes:     " << archive_bytes << '\n'
        << "Transforms:\n";

    for (const auto& entry : transform_usage) {
        std::cout
            << "  " << transform_name(entry.first)
            << " (" << static_cast<unsigned int>(entry.first) << "): "
            << entry.second << " blocks\n";
    }

    return 0;
}

// DAY 23 - Multi-file extraction: restore the whole tree under the
// destination, refusing any recorded path that escapes it.
int extract_multi_file(const std::filesystem::path& archive,
                       const std::filesystem::path& output_dir,
                       bool quiet) {
    const auto tree = uatc::MultiFileArchiveReader::read(archive);

    for (const auto& file : tree.files) {
        const auto relative = uatc::core::safe_relative_path(file.relative_path);
        const auto destination = output_dir / relative;
        if (destination.has_parent_path()) {
            std::filesystem::create_directories(destination.parent_path());
        }
        std::ofstream out(destination, std::ios::binary);
        if (!out) {
            std::cerr << "error: E_IO: cannot write: "
                      << destination.string() << '\n';
            return 5;
        }
        out.write(reinterpret_cast<const char*>(file.data.data()),
                  static_cast<std::streamsize>(file.data.size()));
        if (!out) {
            std::cerr << "error: E_IO: write failed: "
                      << destination.string() << '\n';
            return 5;
        }
        if (!quiet) {
            std::cout
                << "Extracted:         " << destination.string() << '\n';
        }
    }

    if (!quiet) {
        std::cout << "Files:             " << tree.files.size() << '\n';
    }
    return 0;
}

void print_help() {
    std::cout
        << "UATC - Universal Adaptive Transform Compression\n\n"
        << "Usage:\n"
        << "  uatc --help\n"
        << "  uatc stats <input> [--block-size BYTES]\n"
        << "  uatc compress <input> <output.uatc> [--block-size BYTES] [--workers N] [--quiet]\n"
        << "  uatc extract <archive.uatc> <output-dir> [--quiet]\n"
        << "  uatc info <archive.uatc>\n"
        << "  uatc verify <archive.uatc>\n"
        << "  uatc benchmark <dir> [--block-size BYTES] [--workers N]\n"
        << "                   [--gzip] [--out FILE]\n"
        << "  uatc serve [--port N] [--root DIR] [--web DIR] [--allow-origin URL]\n\n"
        << "Commands:\n"
        << "  benchmark Runs the compressor over every file in a directory.\n"
        << "  stats    Analyse a file and report what the engine will do to it.\n"
        << "  compress  Compress a file into a .uatc archive.\n"
        << "  extract   Restore the contents of a .uatc archive.\n"
        << "  info      Display archive metadata without decoding payloads.\n"
        << "  verify    Validate archive structure and payload integrity.\n"
        << "  serve     Start the local HTTP API for the web frontend\n"
        << "            (127.0.0.1 only; paths limited to --root, default the\n"
        << "            current folder; prints a session token).\n\n"
        << "Options:\n"
        << "  --block-size BYTES   Block size to split the input into "
        << "(default 1 MiB).\n"
        << "  --quiet              Print only errors.\n\n"
        << "Progress goes to stderr, results go to stdout, so the output\n"
        << "can be piped and scripted.\n";
}

const char* codec_name(uatc::CodecId id) {
    switch (id) {
        case uatc::CodecId::None:
            return "None";
        case uatc::CodecId::Baseline:
            return "Baseline";
        default:
            return "Unknown";
    }
}

std::size_t parse_size(const std::string& value, const char* option) {
    try {
        const auto parsed = std::stoull(value);
        if (parsed == 0) throw std::invalid_argument("zero");
        return static_cast<std::size_t>(parsed);
    } catch (const std::exception&) {
        throw std::runtime_error(
            std::string("invalid value for ") + option + ": " + value
        );
    }
}

int print_info(const std::filesystem::path& path) {
    const auto info = uatc::ArchiveInspector::inspect(path);

    std::cout
        << "Archive:           " << info.path.string() << '\n'
        << "Format version:    " << info.formatVersion << ".0\n"
        << "Files:             " << info.fileCount << '\n'
        << "Blocks:            " << info.blockCount << '\n'
        << "Original bytes:    " << info.originalBytes << '\n'
        << "Archive bytes:     " << info.archiveBytes << '\n';

    std::cout << "Ratio:             ";

    if (info.archiveBytes == 0) {
        std::cout << "N/A\n";
    } else {
        std::cout << std::fixed << std::setprecision(2)
                  << static_cast<double>(info.originalBytes) /
                     static_cast<double>(info.archiveBytes)
                  << '\n';
    }

    std::cout
        << "Stored blocks:     " << info.storedBlocks << '\n'
        << "Compressed blocks: " << info.compressedBlocks << '\n';

    std::cout << "Transforms:\n";

    for (const auto& usage : info.transformUsage) {
        std::cout
            << "  " << transform_name(usage.id)
            << " (" << static_cast<unsigned int>(usage.id) << "): "
            << usage.blocks << " blocks\n";
    }

    std::cout << "Codecs:\n";

    for (const auto& usage : info.codecUsage) {
        std::cout
            << "  " << codec_name(usage.id)
            << " (" << static_cast<unsigned int>(usage.id) << "): "
            << usage.blocks << " blocks\n";
    }

    return 0;
}

int print_verify(const std::filesystem::path& path) {
    const auto result = uatc::ArchiveVerifier::verify(path);

    if (!result.success) {
        if (result.message == "Block checksum mismatch.") {
            std::cerr << "error: E_BLOCK_CHECKSUM: "
                      << result.message << '\n';
            return 13;
        }

        if (result.message == "Unsupported UATC archive version.") {
            std::cerr << "error: E_UNSUPPORTED_VERSION: "
                      << result.message << '\n';
            return 11;
        }

        std::cerr << "error: E_MALFORMED_ARCHIVE: "
                  << result.message << '\n';
        return 10;
    }

    std::cout
        << "OK: " << result.filesChecked << " file(s), "
        << result.blocksChecked << " block(s) verified.\n";

    return 0;
}

// Prints the per-block progress of a compression run to stderr, so that
// stdout stays clean for the final result.
class ConsoleProgressSink : public uatc::core::ProgressSink {
 public:
    void on_block(const uatc::core::BlockProgress& p) override {
        ++blocks_;
        bytesIn_ += p.bytesIn;
        bytesOut_ += p.bytesOut;

        const double percent =
            p.blockCount ? (100.0 * static_cast<double>(blocks_) /
                            static_cast<double>(p.blockCount))
                         : 0.0;

        std::cerr << "\r  block " << std::setw(4) << blocks_ << "/"
                  << p.blockCount << "  " << std::setw(10) << p.bytesIn
                  << " -> " << std::setw(10) << p.bytesOut << " B  "
                  << std::setw(11) << std::left
                  << (p.stored ? "stored" : transform_label(p.transformId))
                  << std::right << "  " << std::fixed << std::setprecision(1)
                  << percent << "%   " << std::flush;
    }

    void finish() {
        if (blocks_ > 0) std::cerr << '\n';
    }

 private:
    std::uint64_t blocks_ = 0;
    std::uint64_t bytesIn_ = 0;
    std::uint64_t bytesOut_ = 0;
};

int print_stats(const std::filesystem::path& input, std::size_t block_size) {
    uatc::core::CompressOptions options;
    options.block_size = block_size;

    const auto report = uatc::core::analyze(input, options);

    std::cout
        << "File:              " << input.string() << '\n'
        << "Size:              " << report.totalBytes << " bytes\n"
        << "Block size:        " << block_size << " bytes\n"
        << "Blocks:            " << report.blockCount << '\n'
        << "Entropy:           " << std::fixed << std::setprecision(3)
        << report.overallEntropy << " bits/byte\n"
        << "Compressible:      " << report.compressibleBlocks << " block(s)\n"
        << "Stored as-is:      " << report.storedBlocks << " block(s)\n"
        << "Estimated payload: " << report.estimatedArchiveBytes << " bytes\n"
        << "Estimated ratio:   " << std::fixed << std::setprecision(2)
        << report.estimatedRatio << '\n';

    std::cout
        << "\nThe estimate is the fast cost heuristic used to rank the candidate\n"
           "transforms against each other. It is not a size prediction: the real\n"
           "archive can come out either smaller or larger than this.\n";

    std::cout << "\nPer block:\n"
              << "  #   offset      size   entropy  unique  transform"
              << "        estimated\n";

    for (const auto& block : report.blocks) {
        std::cout
            << "  " << std::setw(2) << block.index
            << "  " << std::setw(9) << block.offset
            << "  " << std::setw(8) << block.size
            << "  " << std::fixed << std::setprecision(3) << std::setw(7)
            << block.entropy
            << "  " << std::setw(6) << block.uniqueBytes
            << "  " << std::setw(12) << std::left
            << (block.stored ? "stored" : transform_label(block.transformId))
            << std::right << "  " << std::setw(11)
            << block.estimatedCost << '\n';
    }

    return 0;
}
// Runs the benchmark over a directory of sample files (NFR-02, NFR-07):
// ratio, time and throughput per file, with a gzip comparison on the same
// file when the gzip tool is available. Records are written as JSONL so
// they can be plotted or diffed between runs.
int benchmark_dir(const std::filesystem::path& dir,
                  std::size_t block_size,
                  std::size_t workers,
                  bool with_gzip,
                  const std::filesystem::path& out_file) {
    if (!std::filesystem::exists(dir)) {
        std::cerr << "error: E_NOT_FOUND: directory does not exist: "
                  << dir.string() << '\n';
        return 3;
    }
    if (!std::filesystem::is_directory(dir)) {
        std::cerr << "error: E_INVALID_ARGUMENT: not a directory: "
                  << dir.string() << '\n';
        return 2;
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file()) files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
        return a.generic_string() < b.generic_string();
    });

    const auto scratch = out_file.parent_path() / "uatc_bench_tmp";
    std::filesystem::create_directories(scratch);

    std::ofstream jsonl(out_file, std::ios::binary);
    if (!jsonl) {
        std::cerr << "error: E_IO: cannot write: " << out_file.string() << '\n';
        return 5;
    }

    std::cout << "UATC benchmark\n"
              << "  files:        " << files.size() << '\n'
              << "  block size:   " << block_size << " bytes\n"
              << "  workers:      " << workers << '\n';

    int rc = 0;
    for (const auto& file : files) {
        const auto name = file.filename().string();
        const auto archive = scratch / (name + ".uatc");
        const auto restored_dir = scratch / ("r_" + name);

        auto started = std::chrono::steady_clock::now();
        uatc::core::CompressOptions options;
        options.block_size = block_size;
        options.workers = workers;
        uatc::core::CompressionManager::compress(file, archive, options);
        auto elapsed_compress =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count();

        started = std::chrono::steady_clock::now();
        const auto restored =
            uatc::core::CompressionManager::extract(archive, restored_dir);
        auto elapsed_extract =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count();

        // The extract must reproduce the file, or the benchmark record is
        // not honest about correctness.
        const bool identical =
            read_file_bytes(restored.destination) == read_file_bytes(file);

        const auto original = std::filesystem::file_size(file);
        const auto archived = std::filesystem::file_size(archive);
        const double ratio = archived ? static_cast<double>(original) /
                                        static_cast<double>(archived) : 0.0;
        const double mbps_c =
            elapsed_compress > 0
                ? (original / 1e6) / (elapsed_compress / 1e3) : 0.0;
        const double mbps_d =
            elapsed_extract > 0
                ? (original / 1e6) / (elapsed_extract / 1e3) : 0.0;

        std::string gzip_ratio_field = "null";
        if (with_gzip) {
            const auto gz = scratch / (name + ".gz");
            const std::string cmd =
                "gzip -c -6 '" + file.string() + "' > '" + gz.string() + "'";
            if (std::system(cmd.c_str()) == 0) {
                const auto gzipped = std::filesystem::file_size(gz);
                std::ostringstream o;
                o << std::fixed << std::setprecision(2)
                  << static_cast<double>(original) /
                     static_cast<double>(gzipped);
                gzip_ratio_field = o.str();
            }
            std::filesystem::remove(gz);
        }

        std::cout << "  " << std::left << std::setw(24) << name
                  << std::right
                  << std::setw(12) << original << " -> "
                  << std::setw(12) << archived << " B  ratio "
                  << std::fixed << std::setprecision(2) << std::setw(6)
                  << ratio << "  gzip "
                  << std::setw(6) << gzip_ratio_field << "  "
                  << std::setprecision(1) << std::setw(8) << mbps_c
                  << " MB/s comp  "
                  << std::setw(8) << mbps_d << " MB/s decomp  "
                  << (identical ? "ok" : "ROUND-TRIP FAILED") << '\n';

        if (!identical) rc = 1;

        jsonl << "{\"file\":\"" << name << "\",\"original\":" << original
              << ",\"archive\":" << archived
              << ",\"ratio\":" << ratio
              << ",\"gzip_ratio\":" << gzip_ratio_field
              << ",\"compress_ms\":" << elapsed_compress
              << ",\"extract_ms\":" << elapsed_extract
              << ",\"mbps_compress\":" << mbps_c
              << ",\"mbps_extract\":" << mbps_d
              << ",\"roundtrip\":\"" << (identical ? "ok" : "failed")
              << "\"}\n";

        std::filesystem::remove_all(restored_dir);
        std::filesystem::remove(archive);
    }

    jsonl.close();
    std::filesystem::remove_all(scratch);
    return rc;
}

int compress_file(const std::filesystem::path& input,
                  const std::filesystem::path& output,
                  std::size_t block_size,
                  std::size_t workers,
                  bool quiet) {
    uatc::core::CompressOptions options;
    options.block_size = block_size;
    options.workers = workers;

    ConsoleProgressSink sink;
    const auto result = uatc::core::CompressionManager::compress(
        input, output, options, quiet ? nullptr : &sink);
    sink.finish();

    if (quiet) {
        return 0;
    }

    std::cout
        << "Compressed:        " << input.string() << '\n'
        << "Archive:           " << output.string() << '\n'
        << "Original bytes:    " << result.originalBytes << '\n'
        << "Archive bytes:     " << result.archiveBytes << '\n'
        << "Blocks:            " << result.blockCount << '\n';

    std::cout << "Ratio:             ";

    if (result.archiveBytes == 0) {
        std::cout << "N/A\n";
    } else {
        std::cout << std::fixed << std::setprecision(2)
                  << result.ratio << '\n';
    }

    std::cout
        << "Stored blocks:     " << result.storedBlocks << '\n'
        << "Compressed blocks: " << result.compressedBlocks << '\n'
        << "Transforms:\n";

    for (const auto& entry : result.transformUsage) {
        std::cout
            << "  " << transform_name(entry.first)
            << " (" << static_cast<unsigned int>(entry.first) << "): "
            << entry.second << " blocks\n";
    }

    return 0;
}

int extract_archive(const std::filesystem::path& archive,
                    const std::filesystem::path& output_dir,
                    bool quiet) {
    const auto result =
        uatc::core::CompressionManager::extract(archive, output_dir);

    if (!quiet) {
        std::cout
            << "Extracted:         " << result.destination.string() << '\n'
            << "Bytes written:     " << result.bytesWritten << '\n';
    }

    return 0;
}

// Reads a whole file, for the round-trip comparison in the benchmark.
std::vector<std::uint8_t> read_file_bytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<std::uint8_t>(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());
}

// The API server: `uatc serve` starts the localhost control plane and
// blocks until interrupted. It prints the session token once; every API
// call except /api/health must carry it (SAD 3.9, TC-SEC-03).
int serve(const uatc::server::ServerOptions& options) {
    uatc::server::ApiServer server(options);
    const std::string base =
        "http://127.0.0.1:" + std::to_string(options.port);
    std::cout
        << "UATC API server listening on " << base << "  (Ctrl-C to stop)\n"
        << "  allowed folder: " << server.allowed_root().string() << '\n'
        << "  session token:  " << server.token() << '\n';
    if (!options.web_root.empty()) {
        std::cout << "  open the app:   " << base << "/?live#token="
                  << server.token() << '\n';
    }
    std::cout << std::flush;
    server.serve_forever();
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        print_help();
        return 0;
    }

    // serve [--port N] [--root DIR] [--web DIR] [--allow-origin URL]...
    if (argc >= 2 && std::string(argv[1]) == "serve") {
        uatc::server::ServerOptions options;
        options.web_root = "web";
        try {
            for (int i = 2; i < argc; ++i) {
                const std::string option = argv[i];
                if (i + 1 >= argc) {
                    std::cerr << "error: E_INVALID_ARGUMENT: " << option
                              << " needs a value.\n";
                    return 2;
                }
                const std::string value = argv[++i];
                if (option == "--port") {
                    const auto port = std::stoul(value);
                    if (port == 0 || port > 65535) {
                        throw std::invalid_argument("port");
                    }
                    options.port = static_cast<std::uint16_t>(port);
                } else if (option == "--root") {
                    options.allowed_root = value;
                } else if (option == "--web") {
                    options.web_root = value;
                } else if (option == "--allow-origin") {
                    options.allowed_origins.push_back(value);
                } else {
                    std::cerr << "error: E_INVALID_ARGUMENT: unknown option "
                              << option << '\n';
                    return 2;
                }
            }
        } catch (const std::exception&) {
            std::cerr << "error: E_INVALID_ARGUMENT: --port must be 1-65535.\n";
            return 2;
        }
        if (!std::filesystem::is_directory(options.web_root)) {
            options.web_root.clear();   // API only; no frontend to serve
        }
        try {
            return serve(options);
        } catch (const std::exception& error) {
            std::cerr << "error: E_IO: " << error.what() << '\n';
            return 5;
        }
    }

    if (argc < 3) {
        print_help();
        std::cerr
            << "\nerror: E_INVALID_ARGUMENT: expected a command and its arguments.\n";
        return 2;
    }

    const std::string command = argv[1];

    try {
        if (command == "stats") {
            if (argc < 3) {
                print_help();
                std::cerr << "\nerror: E_INVALID_ARGUMENT: "
                          << "stats needs an input path.\n";
                return 2;
            }

            const std::filesystem::path input = argv[2];
            std::size_t block_size = uatc::core::kDefaultBlockSize;

            for (int i = 3; i < argc; ++i) {
                if (std::string(argv[i]) == "--block-size" && i + 1 < argc) {
                    try {
                        block_size = parse_size(argv[++i], "--block-size");
                    } catch (const std::exception& error) {
                        std::cerr << "error: E_INVALID_ARGUMENT: "
                                  << error.what() << '\n';
                        return 2;
                    }
                } else {
                    print_help();
                    std::cerr << "\nerror: E_INVALID_ARGUMENT: unknown option: "
                              << argv[i] << '\n';
                    return 2;
                }
            }

            return print_stats(input, block_size);
        }

        if (command == "compress") {
            if (argc < 4) {
                print_help();
                std::cerr << "\nerror: E_INVALID_ARGUMENT: "
                          << "compress needs an input and an output path.\n";
                return 2;
            }

            const std::filesystem::path input = argv[2];
            const std::filesystem::path output = argv[3];

            std::size_t block_size = uatc::core::kDefaultBlockSize;
            std::size_t workers = 1;
            bool quiet = false;

            for (int i = 4; i < argc; ++i) {
                const std::string option = argv[i];
                if (option == "--quiet") {
                    quiet = true;
                } else if (option == "--block-size") {
                    if (i + 1 >= argc) {
                        std::cerr << "error: E_INVALID_ARGUMENT: "
                                  << "--block-size needs a value.\n";
                        return 2;
                    }
                    try {
                        block_size = parse_size(argv[++i], "--block-size");
                    } catch (const std::exception& error) {
                        std::cerr << "error: E_INVALID_ARGUMENT: "
                                  << error.what() << '\n';
                        return 2;
                    }
                } else if (option == "--workers") {
                    if (i + 1 >= argc) {
                        std::cerr << "error: E_INVALID_ARGUMENT: "
                                  << "--workers needs a value.\n";
                        return 2;
                    }
                    try {
                        workers = parse_size(argv[++i], "--workers");
                    } catch (const std::exception& error) {
                        std::cerr << "error: E_INVALID_ARGUMENT: "
                                  << error.what() << '\n';
                        return 2;
                    }
                } else {
                    print_help();
                    std::cerr << "\nerror: E_INVALID_ARGUMENT: unknown option: "
                              << option << '\n';
                    return 2;
                }
            }

            if (std::filesystem::is_directory(input)) {
                return compress_directory(input, output, block_size,
                                          workers, quiet);
            }
            return compress_file(input, output, block_size, workers, quiet);
        }

        if (command == "extract") {
            if (argc < 4) {
                print_help();
                std::cerr << "\nerror: E_INVALID_ARGUMENT: "
                          << "extract needs an archive and an output directory.\n";
                return 2;
            }

            const std::filesystem::path archive = argv[2];
            const std::filesystem::path output_dir = argv[3];

            bool quiet = false;
            for (int i = 4; i < argc; ++i) {
                if (std::string(argv[i]) == "--quiet") {
                    quiet = true;
                } else {
                    print_help();
                    std::cerr << "\nerror: E_INVALID_ARGUMENT: unknown option: "
                              << argv[i] << '\n';
                    return 2;
                }
            }

            // A multi-file archive (file_count > 1) restores as a tree; a
            // single-file archive restores as one file. Distinguish by
            // reading the header's file count, not by trying and failing.
            std::uint32_t file_count = 1;
            {
                std::ifstream probe(archive, std::ios::binary);
                if (probe) {
                    probe.seekg(8);  // magic 0, version 4, header_size 6
                    char fc[4] = {};
                    probe.read(fc, 4);
                    file_count = static_cast<std::uint32_t>(
                        static_cast<unsigned char>(fc[0])) |
                        (static_cast<std::uint32_t>(
                            static_cast<unsigned char>(fc[1])) << 8) |
                        (static_cast<std::uint32_t>(
                            static_cast<unsigned char>(fc[2])) << 16) |
                        (static_cast<std::uint32_t>(
                            static_cast<unsigned char>(fc[3])) << 24);
                }
            }
            if (file_count > 1) {
                return extract_multi_file(archive, output_dir, quiet);
            }
            return extract_archive(archive, output_dir, quiet);
        }

        if (command == "benchmark") {
            if (argc < 3) {
                print_help();
                std::cerr << "\nerror: E_INVALID_ARGUMENT: "
                          << "benchmark needs a sample directory.\n";
                return 2;
            }

            const std::filesystem::path dir = argv[2];
            std::size_t block_size = uatc::core::kDefaultBlockSize;
            std::size_t workers = 1;
            bool with_gzip = false;
            std::filesystem::path out_file = "bench_results.jsonl";

            for (int i = 3; i < argc; ++i) {
                const std::string option = argv[i];
                if (option == "--gzip") {
                    with_gzip = true;
                } else if (option == "--block-size" && i + 1 < argc) {
                    try {
                        block_size = parse_size(argv[++i], "--block-size");
                    } catch (const std::exception& error) {
                        std::cerr << "error: E_INVALID_ARGUMENT: "
                                  << error.what() << '\n';
                        return 2;
                    }
                } else if (option == "--workers" && i + 1 < argc) {
                    try {
                        workers = parse_size(argv[++i], "--workers");
                    } catch (const std::exception& error) {
                        std::cerr << "error: E_INVALID_ARGUMENT: "
                                  << error.what() << '\n';
                        return 2;
                    }
                } else if (option == "--out" && i + 1 < argc) {
                    out_file = argv[++i];
                } else {
                    print_help();
                    std::cerr << "\nerror: E_INVALID_ARGUMENT: unknown option: "
                              << option << '\n';
                    return 2;
                }
            }

            return benchmark_dir(dir, block_size, workers, with_gzip, out_file);
        }

        if (command != "info" && command != "verify") {
            print_help();
            std::cerr << "\nerror: E_INVALID_ARGUMENT: unknown command: "
                      << command << '\n';
            return 2;
        }

        if (argc != 3) {
            print_help();
            std::cerr << "\nerror: E_INVALID_ARGUMENT: "
                      << "expected a command and archive path.\n";
            return 2;
        }

        const std::filesystem::path archive_path = argv[2];

        if (!std::filesystem::exists(archive_path)) {
            std::cerr << "error: E_NOT_FOUND: archive does not exist: "
                      << archive_path.string() << '\n';
            return 3;
        }

        if (!std::filesystem::is_regular_file(archive_path)) {
            std::cerr << "error: E_INVALID_ARGUMENT: "
                      << "archive path is not a file.\n";
            return 2;
        }

        if (command == "info") {
            return print_info(archive_path);
        }

        return print_verify(archive_path);

    } catch (const uatc::core::UatcError& error) {
        std::cerr << "error: " << uatc::core::error_code_name(error.code())
                  << ": " << error.what() << '\n';
        return uatc::core::error_exit_code(error.code());

    } catch (const std::filesystem::filesystem_error& error) {
        std::cerr << "error: E_IO: " << error.what() << '\n';
        return 5;

    } catch (const std::exception& error) {
        std::cerr << "error: E_MALFORMED_ARCHIVE: "
                  << error.what() << '\n';
        return 10;
    }
}
