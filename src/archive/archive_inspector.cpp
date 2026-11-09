
#include "archive/archive_inspector.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

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
    if (archive_size < UATC_INTEGRITY_REGION_SIZE) {
        throw std::runtime_error(
            "Archive is truncated or missing its integrity footer."
        );
    }
    const std::uint64_t data_region_size =
        archive_size - UATC_INTEGRITY_REGION_SIZE;

    BinaryReader reader(archive_path);
    reader.seekg(archive_size - UATC_ARCHIVE_FOOTER_SIZE);
    if (reader.read_u64() != 0 || reader.read_u32() != UATC_MAGIC) {
        throw std::runtime_error("Archive integrity footer is invalid.");
    }
    reader.seekg(0);

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
    if (header.file_table_offset < HEADER_SIZE ||
        header.file_table_offset > data_region_size ||
        header.block_index_offset < header.file_table_offset ||
        header.block_index_offset > data_region_size ||
        header.payload_offset < header.block_index_offset ||
        header.payload_offset > data_region_size) {
        throw std::runtime_error("Invalid archive section offsets.");
    }

    const std::uint64_t file_table_end =
        checked_add(
            header.file_table_offset,
            checked_multiply(header.file_count, FILE_RECORD_SIZE)
        );
    if (file_table_end > header.block_index_offset) {
        throw std::runtime_error("File table extends beyond its valid region.");
    }

    const std::uint64_t block_index_size =
        checked_multiply(header.block_count, BLOCK_RECORD_SIZE);
    const std::uint64_t block_index_end =
        checked_add(header.block_index_offset, block_index_size);

    if (block_index_end > data_region_size) {
        throw std::runtime_error("Block index extends beyond archive data.");
    }
    if (header.payload_offset < block_index_end) {
        throw std::runtime_error("Payload overlaps the block index.");
    }

    struct FileRecordInfo {
        std::uint64_t size = 0;
        std::uint32_t first_block = 0;
        std::uint32_t block_count = 0;
        std::uint64_t decoded_bytes = 0;
        std::string path;
    };

    std::vector<FileRecordInfo> files(header.file_count);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> path_ranges;
    std::vector<std::string> paths;
    path_ranges.reserve(header.file_count);
    paths.reserve(header.file_count);

    ArchiveInfo info;
    info.path = archive_path;
    info.formatVersion = header.version;
    info.fileCount = header.file_count;
    info.blockCount = header.block_count;
    info.archiveBytes = archive_size;
    info.files.reserve(header.file_count);

    std::uint64_t next_first_block = 0;
    for (std::uint32_t file_id = 0; file_id < header.file_count; ++file_id) {
        const std::uint64_t record_offset = checked_add(
            header.file_table_offset,
            checked_multiply(file_id, FILE_RECORD_SIZE)
        );
        reader.seekg(record_offset);
        const std::uint32_t recorded_id = reader.read_u32();
        const std::uint64_t path_offset = reader.read_u64();
        const std::uint32_t path_length = reader.read_u32();
        const std::uint64_t original_size = reader.read_u64();
        const std::uint32_t first_block = reader.read_u32();
        const std::uint32_t file_block_count = reader.read_u32();

        if (recorded_id != file_id || first_block != next_first_block) {
            throw std::runtime_error(
                "Invalid file ID or first-block index."
            );
        }
        next_first_block = checked_add(next_first_block, file_block_count);
        if (next_first_block > header.block_count) {
            throw std::runtime_error(
                "File block counts exceed the archive block count."
            );
        }

        const std::uint64_t path_end = checked_add(path_offset, path_length);
        if (path_length == 0 || path_offset < file_table_end ||
            path_end > header.block_index_offset) {
            throw std::runtime_error(
                "File path extends beyond its valid archive region."
            );
        }
        reader.seekg(path_offset);
        const std::string path = reader.read_string(path_length);
        if (path.find('\0') != std::string::npos) {
            throw std::runtime_error("File path contains an embedded NUL.");
        }
        files[file_id] = {
            original_size,
            first_block,
            file_block_count,
            0,
            path
        };
        info.originalBytes = checked_add(info.originalBytes, original_size);
        info.files.push_back({
            file_id,
            path,
            original_size,
            file_block_count
        });
        paths.push_back(path);
        path_ranges.emplace_back(path_offset, path_end);
    }

    if (next_first_block != header.block_count) {
        throw std::runtime_error(
            "File block counts do not match the archive block count."
        );
    }
    std::sort(path_ranges.begin(), path_ranges.end());
    for (std::size_t i = 1; i < path_ranges.size(); ++i) {
        if (path_ranges[i].first < path_ranges[i - 1].second) {
            throw std::runtime_error("File paths overlap in the archive.");
        }
    }
    std::sort(paths.begin(), paths.end());
    if (std::adjacent_find(paths.begin(), paths.end()) != paths.end()) {
        throw std::runtime_error("Archive contains duplicate file paths.");
    }

    std::uint64_t next_payload_offset = header.payload_offset;
    std::uint32_t current_file = 0;
    reader.seekg(header.block_index_offset);

    for (std::uint32_t i = 0; i < header.block_count; ++i) {
        while (current_file < header.file_count &&
               i >= files[current_file].first_block +
                        files[current_file].block_count) {
            ++current_file;
        }
        if (current_file >= header.file_count) {
            throw std::runtime_error("Block index refers to no file record.");
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

        const std::uint32_t expected_block_id =
            i - files[current_file].first_block;
        if (block_file_id != current_file) {
            throw std::runtime_error("Invalid block file ID.");
        }
        if (block_id != expected_block_id) {
            throw std::runtime_error(
                "Per-file block IDs are not sequential."
            );
        }
        if (!is_known_transform(transform_id)) {
            throw std::runtime_error("Unsupported transform.");
        }
        if (codec_id != CodecId::None &&
            codec_id != CodecId::Baseline) {
            throw std::runtime_error("Unsupported codec.");
        }
        if (transform_parameter != 0) {
            throw std::runtime_error("Unsupported transform parameter.");
        }
        if (original_length == 0 || payload_offset != next_payload_offset) {
            throw std::runtime_error("Invalid block payload range.");
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

        if (payload_end > data_region_size) {
            throw std::runtime_error(
                "Block payload extends beyond archive."
            );
        }
        next_payload_offset = payload_end;
        files[block_file_id].decoded_bytes = checked_add(
            files[block_file_id].decoded_bytes,
            original_length
        );

        add_transform_usage(info, transform_id);
        add_codec_usage(info, codec_id);

        if (codec_id == CodecId::None &&
            transform_id == TransformId::None) {
            ++info.storedBlocks;
        } else {
            ++info.compressedBlocks;
        }
    }

    if (next_payload_offset != data_region_size) {
        throw std::runtime_error(
            "Payload data does not end at the integrity region."
        );
    }
    for (std::size_t file_id = 0; file_id < files.size(); ++file_id) {
        if (files[file_id].decoded_bytes != files[file_id].size) {
            throw std::runtime_error(
                "File block sizes do not match the file table."
            );
        }
    }
    info.ratio = info.originalBytes == 0
        ? 0.0
        : static_cast<double>(archive_size) /
          static_cast<double>(info.originalBytes);

    return info;
}

} // namespace uatc
