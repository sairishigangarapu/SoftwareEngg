
#include "archive/archive_inspector.hpp"

#include <limits>
#include <stdexcept>
#include <unordered_map>

#include "archive/archive_binary.hpp"
#include "core/transform_factory.hpp"

namespace uatc {
namespace {

constexpr std::uint64_t HEADER_SIZE = UATC_ARCHIVE_HEADER_SIZE;
constexpr std::uint64_t FILE_RECORD_SIZE = UATC_FILE_RECORD_SIZE;
constexpr std::uint64_t BLOCK_RECORD_SIZE = UATC_BLOCK_RECORD_SIZE;

std::uint64_t checked_add(std::uint64_t a, std::uint64_t b) {
    if (b > std::numeric_limits<std::uint64_t>::max() - a) {
        throw std::runtime_error("Archive offset overflow.");
    }
    return a + b;
}

std::uint64_t checked_multiply(std::uint64_t a, std::uint64_t b) {
    if (a != 0 &&
        b > std::numeric_limits<std::uint64_t>::max() / a) {
        throw std::runtime_error("Archive size overflow.");
    }
    return a * b;
}

// The name comes from the compression core, so the inspector and the CLI
// always agree with the transform engine.
std::string transform_name(TransformId id) {
    auto transform = core::make_transform(id);
    if (!transform) {
        throw std::runtime_error("Unsupported transform.");
    }
    return transform->name();
}

// True for every transform the compression core registers.
bool is_known_transform(TransformId id) {
    for (TransformId candidate : core::candidate_transform_ids()) {
        if (id == candidate) return true;
    }
    return false;
}

std::string codec_name(CodecId id) {
    switch (id) {
        case CodecId::None: return "Raw";
        case CodecId::Baseline: return "Baseline";
        case CodecId::Huffman: return "Huffman";
    }
    throw std::runtime_error("Unsupported codec.");
}

void add_transform_usage(ArchiveInfo& info, TransformId id) {
    for (auto& usage : info.transformUsage) {
        if (usage.id == id) {
            ++usage.blocks;
            return;
        }
    }

    info.transformUsage.push_back({id, transform_name(id), 1});
}

void add_codec_usage(ArchiveInfo& info, CodecId id) {
    for (auto& usage : info.codecUsage) {
        if (usage.id == id) {
            ++usage.blocks;
            return;
        }
    }

    info.codecUsage.push_back({id, codec_name(id), 1});
}

} // namespace

ArchiveInfo ArchiveInspector::inspect(
    const std::filesystem::path& archive_path
) {
    if (!std::filesystem::exists(archive_path)) {
        throw std::runtime_error(
            "Archive does not exist: " + archive_path.string()
        );
    }

    if (!std::filesystem::is_regular_file(archive_path)) {
        throw std::runtime_error(
            "Archive path is not a regular file: " + archive_path.string()
        );
    }

    const std::uint64_t archive_size =
        std::filesystem::file_size(archive_path);

    BinaryReader reader(archive_path);

    ArchiveHeader header;
    header.magic = reader.read_u32();
    header.version = reader.read_u16();
    header.header_size = reader.read_u16();
    header.file_count = reader.read_u32();
    header.block_count = reader.read_u32();
    header.file_table_offset = reader.read_u64();
    header.block_index_offset = reader.read_u64();
    header.payload_offset = reader.read_u64();

    if (header.magic != UATC_MAGIC) {
        throw std::runtime_error("Invalid UATC archive magic.");
    }
    if (header.version != UATC_VERSION) {
        throw std::runtime_error("Unsupported UATC archive version.");
    }
    if (header.header_size != HEADER_SIZE) {
        throw std::runtime_error("Invalid UATC header size.");
    }
    // A single-file archive has one file record; a folder archive (Day 20,
    // multi_file.cpp) has one per file, in file-table order. Both share the
    // header, the record layouts and the block index, so one reader serves
    // both (FR-11 for folder archives).
    if (header.file_count == 0) {
        throw std::runtime_error("Archive has no files.");
    }
    if (header.file_table_offset < HEADER_SIZE ||
        header.block_index_offset < header.file_table_offset ||
        header.payload_offset < header.block_index_offset) {
        throw std::runtime_error("Invalid archive section offsets.");
    }

    // Bounds before any allocation: every count the header declares must
    // fit in the bytes that are actually there (TC-SEC-02).
    const std::uint64_t file_table_end = checked_add(
        header.file_table_offset,
        checked_multiply(header.file_count, FILE_RECORD_SIZE));
    if (file_table_end > archive_size ||
        file_table_end > header.block_index_offset) {
        throw std::runtime_error("File table extends beyond archive.");
    }

    const std::uint64_t block_index_size =
        checked_multiply(header.block_count, BLOCK_RECORD_SIZE);
    const std::uint64_t block_index_end =
        checked_add(header.block_index_offset, block_index_size);

    if (block_index_end > archive_size) {
        throw std::runtime_error("Block index extends beyond archive.");
    }
    if (header.payload_offset < block_index_end) {
        throw std::runtime_error("Payload overlaps the block index.");
    }

    ArchiveInfo info;
    info.path = archive_path;
    info.formatVersion = header.version;
    info.fileCount = header.file_count;
    info.blockCount = header.block_count;
    info.archiveBytes = archive_size;

    // File records: ids in order, blocks contiguous and covering the index.
    std::uint64_t next_block = 0;
    for (std::uint32_t f = 0; f < header.file_count; ++f) {
        reader.seekg(header.file_table_offset + f * FILE_RECORD_SIZE);

        const std::uint32_t file_id = reader.read_u32();
        const std::uint64_t path_offset = reader.read_u64();
        const std::uint32_t path_length = reader.read_u32();
        const std::uint64_t original_size = reader.read_u64();
        const std::uint32_t first_block = reader.read_u32();
        const std::uint32_t file_block_count = reader.read_u32();

        if (file_id != f) {
            throw std::runtime_error("Invalid file ID.");
        }
        if (file_block_count != 0 && first_block != next_block) {
            throw std::runtime_error("Invalid first block index.");
        }
        next_block = checked_add(next_block, file_block_count);
        if (next_block > header.block_count) {
            throw std::runtime_error("File/block count mismatch.");
        }

        const std::uint64_t path_end = checked_add(path_offset, path_length);
        if (path_length == 0 ||
            path_offset < file_table_end ||
            path_end > archive_size ||
            path_end > header.block_index_offset) {
            throw std::runtime_error(
                "File path extends beyond its valid archive region."
            );
        }

        reader.seekg(path_offset);
        ArchivedFileInfo file;
        file.path = reader.read_string(path_length);
        if (file.path.empty()) {
            throw std::runtime_error("Archive contains an empty file path.");
        }
        file.originalBytes = original_size;
        file.blocks = file_block_count;

        info.originalBytes = checked_add(info.originalBytes, original_size);
        info.files.push_back(std::move(file));
    }
    if (next_block != header.block_count) {
        throw std::runtime_error("File/block count mismatch.");
    }

    // Same definition as the compressor, the CLI and the API:
    // original / archive, so 2.0 means "half the size".
    info.ratio = archive_size == 0
        ? 0.0
        : static_cast<double>(info.originalBytes) /
          static_cast<double>(archive_size);

    reader.seekg(header.block_index_offset);

    // Which file each block belongs to, and its index inside that file.
    // The block index was bounds-checked against the archive size above,
    // so block_count is at most archive_size / BLOCK_RECORD_SIZE here.
    std::uint32_t current_file = 0;
    std::uint64_t file_block_start = 0;

    for (std::uint32_t i = 0; i < header.block_count; ++i) {
        while (current_file < info.files.size() &&
               i >= file_block_start + info.files[current_file].blocks) {
            file_block_start += info.files[current_file].blocks;
            ++current_file;
        }
        const std::uint32_t block_file_id = reader.read_u32();
        const std::uint32_t block_id = reader.read_u32();
        const std::uint64_t payload_offset = reader.read_u64();
        const std::uint64_t original_length = reader.read_u64();
        const std::uint64_t stored_length = reader.read_u64();
        const auto transform_id =
            static_cast<TransformId>(reader.read_u8());
        const auto codec_id =
            static_cast<CodecId>(reader.read_u8());
        const std::uint16_t transform_parameter = reader.read_u16();
        reader.read_u32(); // Checksum is metadata; payload is not decoded.

        if (block_file_id != current_file) {
            throw std::runtime_error("Invalid block file ID.");
        }
        if (block_id != i - file_block_start) {
            throw std::runtime_error("Block IDs are not sequential.");
        }
        if (!is_known_transform(transform_id)) {
            throw std::runtime_error("Unsupported transform.");
        }
        if (codec_id != CodecId::None &&
            codec_id != CodecId::Baseline &&
            codec_id != CodecId::Huffman) {
            throw std::runtime_error("Unsupported codec.");
        }
        if (transform_parameter != 0) {
            throw std::runtime_error("Unsupported transform parameter.");
        }
        if (transform_id == TransformId::None &&
            codec_id == CodecId::None &&
            stored_length != original_length) {
            throw std::runtime_error(
                "Raw block stored length must equal original length."
            );
        }

        const std::uint64_t payload_end =
            checked_add(payload_offset, stored_length);

        if (payload_offset < header.payload_offset ||
            payload_end > archive_size) {
            throw std::runtime_error(
                "Block payload extends beyond archive."
            );
        }

        add_transform_usage(info, transform_id);
        add_codec_usage(info, codec_id);

        if (codec_id == CodecId::None &&
            transform_id == TransformId::None) {
            ++info.storedBlocks;
        } else {
            ++info.compressedBlocks;
        }
    }

    return info;
}

} // namespace uatc
