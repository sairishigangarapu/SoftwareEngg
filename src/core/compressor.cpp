#include "core/compressor.hpp"

#include <fstream>
#include <stdexcept>

#include "archive/archive_reader.hpp"
#include "archive/archive_writer.hpp"
#include "archive/block.hpp"
#include "core/compressibility_probe.hpp"
#include "core/error.hpp"
#include "core/transform_factory.hpp"
#include "core/transform_selector.hpp"
#include "filesystem/file_reader.hpp"

namespace uatc::core {

CompressResult CompressionManager::compress(
    const std::filesystem::path& source,
    const std::filesystem::path& archive,
    const CompressOptions& options
) {
    if (!std::filesystem::exists(source)) {
        throw UatcError(ErrorCode::NotFound,
                        "Input does not exist: " + source.string());
    }
    if (!std::filesystem::is_regular_file(source)) {
        throw UatcError(ErrorCode::InvalidArgument,
                        "Input path is not a regular file: " + source.string());
    }
    if (options.block_size == 0) {
        throw UatcError(ErrorCode::InvalidArgument,
                        "Block size must be greater than zero.");
    }

    const auto data = FileReader::read(source);
    const auto blocks = BlockSplitter::split(data, options.block_size);

    const auto registry = make_default_registry();
    const TransformSelector selector(registry);
    const CompressibilityProbe probe;

    std::vector<BlockEncoding> encodings;
    encodings.reserve(blocks.size());

    CompressResult result;
    result.originalBytes = data.size();
    result.blockCount = blocks.size();

    for (const auto& block : blocks) {
        BlockEncoding encoding;

        if (probe.is_compressible(ByteSpan(block.data))) {
            // Compressible: pick the cheapest transform for this block.
            const auto selection = selector.select(ByteSpan(block.data));
            encoding.transform_id = selection.transform_id;
            encoding.codec_id = CodecId::Baseline;
            ++result.compressedBlocks;
        } else {
            // Incompressible: keep the original bytes (FR-08).
            encoding.transform_id = TransformId::None;
            encoding.codec_id = CodecId::None;
            ++result.storedBlocks;
        }

        ++result.transformUsage[encoding.transform_id];
        encodings.push_back(encoding);
    }

    ArchiveWriter::write(archive, source, blocks, encodings);

    result.archiveBytes = std::filesystem::file_size(archive);
    if (result.archiveBytes > 0) {
        result.ratio = static_cast<double>(result.originalBytes) /
                       static_cast<double>(result.archiveBytes);
    }
    return result;
}

ExtractResult CompressionManager::extract(
    const std::filesystem::path& archive,
    const std::filesystem::path& outputDirectory
) {
    if (!std::filesystem::exists(archive)) {
        throw UatcError(ErrorCode::NotFound,
                        "Archive does not exist: " + archive.string());
    }

    const auto file = ArchiveReader::read_file(archive);

    // The recorded path must stay inside the destination directory: reject
    // absolute paths and any '..' segment (E_PATH_TRAVERSAL).
    const std::filesystem::path relative(file.relative_path);
    if (relative.is_absolute() ||
        relative.string().find("..") != std::string::npos) {
        throw UatcError(ErrorCode::PathTraversal,
                        "The archive contains an unsafe file path: " +
                            file.relative_path);
    }

    std::filesystem::create_directories(outputDirectory);
    const auto destination = outputDirectory / relative;
    if (destination.has_parent_path()) {
        std::filesystem::create_directories(destination.parent_path());
    }

    std::ofstream out(destination, std::ios::binary);
    if (!out) {
        throw UatcError(ErrorCode::Io,
                        "Cannot write: " + destination.string());
    }
    out.write(reinterpret_cast<const char*>(file.data.data()),
              static_cast<std::streamsize>(file.data.size()));
    out.close();
    if (!out) {
        throw UatcError(ErrorCode::Io,
                        "Write failed: " + destination.string());
    }

    ExtractResult result;
    result.destination = destination;
    result.relativePath = file.relative_path;
    result.bytesWritten = file.data.size();
    return result;
}

}  // namespace uatc::core
