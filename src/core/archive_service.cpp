#include "core/archive_service.hpp"

#include <fstream>
#include <map>
#include <system_error>

#include "archive/archive_inspector.hpp"
#include "archive/block.hpp"
#include "archive/multi_file.hpp"
#include "core/compressibility_probe.hpp"
#include "core/error.hpp"
#include "core/path_safety.hpp"
#include "core/thread_pool.hpp"
#include "core/transform_factory.hpp"
#include "core/transform_selector.hpp"
#include "filesystem/file_enumerator.hpp"
#include "filesystem/file_reader.hpp"

namespace uatc::core {

namespace fs = std::filesystem;

namespace {

bool is_stored(const CodedBlock& block) {
    return block.transform_id == TransformId::None &&
           block.codec_id == CodecId::None;
}

// Moved from the CLI (Day 23) so the HTTP API can compress folders too.
CompressResult compress_directory(const fs::path& dir,
                                  const fs::path& archive,
                                  const CompressOptions& options,
                                  ProgressSink* progress) {
    auto entries = FileEnumerator::enumerate(dir);

    // Never pack the archive into itself (e.g. "compress . into ./x.uatc"
    // run twice).
    std::error_code ec;
    const fs::path archive_canonical = fs::weakly_canonical(archive, ec);
    if (!ec) {
        std::vector<FileEntry> kept;
        for (auto& entry : entries) {
            const auto path = fs::weakly_canonical(
                dir / entry.relative_path, ec);
            if (ec || path != archive_canonical) kept.push_back(entry);
        }
        entries.swap(kept);
    }
    if (entries.empty()) {
        throw UatcError(ErrorCode::InvalidArgument,
                        "The folder holds no files to compress: " +
                            dir.string());
    }

    // Total block count up front, so progress is a true fraction.
    std::uint64_t total_blocks = 0;
    for (const auto& entry : entries) {
        total_blocks += (entry.file_size + options.block_size - 1) /
                        options.block_size;
    }

    const auto registry = make_default_registry();
    const TransformSelector selector(registry);
    const CompressibilityProbe probe;
    const std::size_t workers = options.workers > 1 ? options.workers : 1;

    std::vector<ArchiveInput> inputs;
    std::vector<std::vector<CodedBlock>> coded;
    inputs.reserve(entries.size());
    coded.reserve(entries.size());

    CompressResult result;
    result.fileCount = entries.size();
    std::uint64_t reported = 0;

    // Files in sorted order; each file's blocks may run on the pool, and
    // land in a pre-sized vector by index, so the archive is byte-identical
    // for any worker count (FR-10, ADR-05).
    for (const auto& entry : entries) {
        ArchiveInput input;
        input.relative_path = entry.relative_path.generic_string();
        input.data = FileReader::read(dir / entry.relative_path);
        result.originalBytes += input.data.size();

        const auto blocks = BlockSplitter::split(input.data, options.block_size);
        std::vector<CodedBlock> per_file(blocks.size());

        if (workers > 1 && blocks.size() > 1) {
            auto pool = make_thread_pool(workers, workers * 4);
            for (std::size_t b = 0; b < blocks.size(); ++b) {
                pool->submit([&blocks, &selector, &probe, &per_file, b] {
                    per_file[b] = CompressionManager::encode_block(
                        blocks[b], selector, probe);
                });
            }
            pool->wait_idle();
        } else {
            for (std::size_t b = 0; b < blocks.size(); ++b) {
                per_file[b] =
                    CompressionManager::encode_block(blocks[b], selector, probe);
            }
        }

        for (std::size_t b = 0; b < per_file.size(); ++b) {
            const auto& block = per_file[b];
            ++result.blockCount;
            ++result.transformUsage[block.transform_id];
            if (is_stored(block)) {
                ++result.storedBlocks;
            } else {
                ++result.compressedBlocks;
            }
            if (progress != nullptr) {
                BlockProgress p;
                p.blockIndex = reported++;
                p.blockCount = total_blocks;
                p.bytesIn = blocks[b].size();
                p.bytesOut = block.payload.size();
                p.transformId = block.transform_id;
                p.stored = is_stored(block);
                progress->on_block(p);
            }
        }

        coded.push_back(std::move(per_file));
        inputs.push_back(std::move(input));
    }

    MultiFileArchiveWriter::write(archive, inputs, coded, options.block_size);

    result.archiveBytes = fs::file_size(archive);
    if (result.archiveBytes > 0) {
        result.ratio = static_cast<double>(result.originalBytes) /
                       static_cast<double>(result.archiveBytes);
    }
    return result;
}

}  // namespace

CompressResult compress_path(const fs::path& source,
                             const fs::path& archive,
                             const CompressOptions& options,
                             ProgressSink* progress) {
    if (!fs::exists(source)) {
        throw UatcError(ErrorCode::NotFound,
                        "Input does not exist: " + source.string());
    }
    if (options.block_size == 0) {
        throw UatcError(ErrorCode::InvalidArgument,
                        "Block size must be greater than zero.");
    }
    if (fs::is_directory(source)) {
        return compress_directory(source, archive, options, progress);
    }
    auto result = CompressionManager::compress(source, archive, options, progress);
    result.fileCount = 1;
    return result;
}

ExtractSummary extract_path(const fs::path& archive,
                            const fs::path& destination_dir) {
    if (!fs::exists(archive)) {
        throw UatcError(ErrorCode::NotFound,
                        "Archive does not exist: " + archive.string());
    }

    ExtractSummary summary;
    summary.destination = destination_dir;

    // The header's file count decides the reader. The inspector validates
    // the metadata on the way, so a damaged header fails here.
    const auto info = ArchiveInspector::inspect(archive);

    if (info.fileCount <= 1) {
        const auto result = CompressionManager::extract(archive, destination_dir);
        summary.files = 1;
        summary.bytesWritten = result.bytesWritten;
        summary.written.push_back(result.destination);
        return summary;
    }

    // Reads and verifies everything before writing anything.
    const auto tree = MultiFileArchiveReader::read(archive);

    // Check every path first, so an unsafe entry stops the job before a
    // single file is created (TC-SEC-01).
    std::vector<fs::path> targets;
    targets.reserve(tree.files.size());
    for (const auto& file : tree.files) {
        targets.push_back(destination_dir / safe_relative_path(file.relative_path));
    }

    try {
        fs::create_directories(destination_dir);
        for (std::size_t i = 0; i < tree.files.size(); ++i) {
            const auto& target = targets[i];
            if (target.has_parent_path()) {
                fs::create_directories(target.parent_path());
            }
            std::ofstream out(target, std::ios::binary);
            if (!out) {
                throw UatcError(ErrorCode::Io, "Cannot write: " + target.string());
            }
            summary.written.push_back(target);
            const auto& data = tree.files[i].data;
            out.write(reinterpret_cast<const char*>(data.data()),
                      static_cast<std::streamsize>(data.size()));
            out.close();
            if (!out) {
                throw UatcError(ErrorCode::Io, "Write failed: " + target.string());
            }
            summary.bytesWritten += data.size();
            ++summary.files;
        }
    } catch (...) {
        // SAD 4.4: remove the partial output this job created.
        std::error_code ignored;
        for (const auto& path : summary.written) fs::remove(path, ignored);
        throw;
    }
    return summary;
}

}  // namespace uatc::core
