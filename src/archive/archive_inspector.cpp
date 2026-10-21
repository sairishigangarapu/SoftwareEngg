
#include "archive/archive_inspector.hpp"

#include <limits>
#include <stdexcept>
#include <unordered_map>

#include "archive/archive_binary.hpp"

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

std::string transform_name(TransformId id) {
    switch (id) {
        case TransformId::None: return "None";
        case TransformId::RLE: return "RLE";
        case TransformId::Delta: return "Delta";
    }
    throw std::runtime_error("Unsupported transform.");
}

std::string codec_name(CodecId id) {
    switch (id) {
        case CodecId::None: return "Raw";
        case CodecId::Baseline: return "Baseline";
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
    if (header.file_count != 1) {
        throw std::runtime_error(
            "This inspector currently supports one file per archive."
        );
    }
    if (header.file_table_offset < HEADER_SIZE ||
        header.block_index_offset < header.file_table_offset ||
        header.payload_offset < header.block_index_offset) {
        throw std::runtime_error("Invalid archive section offsets.");
    }

    const std::uint64_t file_table_end =
        checked_add(header.file_table_offset, FILE_RECORD_SIZE);

    if (file_table_end > archive_size) {
        throw std::runtime_error("File table extends beyond archive.");
    }

    reader.seekg(header.file_table_offset);

    const std::uint32_t file_id = reader.read_u32();
    const std::uint64_t path_offset = reader.read_u64();
    const std::uint32_t path_length = reader.read_u32();
    const std::uint64_t original_size = reader.read_u64();
    const std::uint32_t first_block = reader.read_u32();
    const std::uint32_t file_block_count = reader.read_u32();

    if (file_id != 0) {
        throw std::runtime_error("Invalid file ID.");
    }
    if (first_block != 0 && header.block_count != 0) {
        throw std::runtime_error("Invalid first block index.");
    }
    if (file_block_count != header.block_count) {
        throw std::runtime_error("File/block count mismatch.");
    }

    const std::uint64_t path_end = checked_add(path_offset, path_length);
    if (path_offset < file_table_end ||
        path_end > archive_size ||
        path_end > header.block_index_offset) {
        throw std::runtime_error(
            "File path extends beyond its valid archive region."
        );
    }

    reader.seekg(path_offset);
    if (reader.read_string(path_length).empty()) {
        throw std::runtime_error("Archive contains an empty file path.");
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
    info.originalBytes = original_size;
    info.archiveBytes = archive_size;
    info.ratio = original_size == 0
        ? 0.0
        : static_cast<double>(archive_size) /
          static_cast<double>(original_size);

    reader.seekg(header.block_index_offset);

    for (std::uint32_t i = 0; i < header.block_count; ++i) {
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

        if (block_file_id != 0) {
            throw std::runtime_error("Invalid block file ID.");
        }
        if (block_id != i) {
            throw std::runtime_error("Block IDs are not sequential.");
        }
        if (transform_id != TransformId::None &&
            transform_id != TransformId::RLE &&
            transform_id != TransformId::Delta) {
            throw std::runtime_error("Unsupported transform.");
        }
        if (codec_id != CodecId::None &&
            codec_id != CodecId::Baseline) {
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
