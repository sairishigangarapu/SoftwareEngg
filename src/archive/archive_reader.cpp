
#include "archive/archive_reader.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "archive/archive_binary.hpp"
#include "archive/codec.hpp"
#include "archive/transforms.hpp"

namespace uatc {

namespace {

constexpr std::uint64_t HEADER_SIZE =
    UATC_ARCHIVE_HEADER_SIZE;

constexpr std::uint64_t FILE_RECORD_SIZE =
    UATC_FILE_RECORD_SIZE;

constexpr std::uint64_t BLOCK_RECORD_SIZE =
    UATC_BLOCK_RECORD_SIZE;

// FNV-1a 32-bit checksum of the original block data.
std::uint32_t calculate_checksum(
    const std::vector<std::uint8_t>& data
) {
    std::uint32_t hash = 2166136261u;

    for (const std::uint8_t byte : data) {
        hash ^= byte;
        hash *= 16777619u;
    }

    return hash;
}

std::uint64_t checked_add(
    std::uint64_t a,
    std::uint64_t b
) {
    if (b > std::numeric_limits<std::uint64_t>::max() - a) {
        throw std::runtime_error("Archive offset overflow.");
    }

    return a + b;
}

std::uint64_t checked_multiply(
    std::uint64_t a,
    std::uint64_t b
) {
    if (a != 0 &&
        b > std::numeric_limits<std::uint64_t>::max() / a) {
        throw std::runtime_error("Archive size overflow.");
    }

    return a * b;
}

} // namespace

void ArchiveReader::validate_header(
    const ArchiveHeader& header
) {
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
            "This reader currently supports one file per archive."
        );
    }

    if (header.file_table_offset < HEADER_SIZE) {
        throw std::runtime_error("Invalid file table offset.");
    }

    if (header.block_index_offset < header.file_table_offset) {
        throw std::runtime_error("Invalid block index offset.");
    }

    if (header.payload_offset < header.block_index_offset) {
        throw std::runtime_error("Invalid payload offset.");
    }
}

void ArchiveReader::validate_block(
    std::uint32_t block_id,
    std::uint32_t file_id,
    std::uint64_t original_length,
    std::uint64_t stored_length,
    TransformId transform_id,
    CodecId codec_id,
    std::uint16_t transform_parameter
) {
    if (file_id != 0) {
        throw std::runtime_error("Invalid block file ID.");
    }

    if (block_id == std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("Invalid block ID.");
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

    // None, RLE, and Delta currently define no parameters.
    if (transform_parameter != 0) {
        throw std::runtime_error(
            "Unsupported transform parameter."
        );
    }

    if (transform_id == TransformId::None &&
        codec_id == CodecId::None &&
        stored_length != original_length) {
        throw std::runtime_error(
            "Raw block stored length must equal original length."
        );
    }
}

std::vector<std::uint8_t> ArchiveReader::read(
    const std::filesystem::path& archive_path
) {
    if (!std::filesystem::exists(archive_path)) {
        throw std::runtime_error(
            "Archive does not exist: " + archive_path.string()
        );
    }

    if (!std::filesystem::is_regular_file(archive_path)) {
        throw std::runtime_error(
            "Archive path is not a regular file: " +
            archive_path.string()
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

    validate_header(header);

    if (header.header_size > archive_size) {
        throw std::runtime_error("Archive is truncated.");
    }

    const std::uint64_t file_table_end =
        checked_add(header.file_table_offset, FILE_RECORD_SIZE);

    if (file_table_end > archive_size) {
        throw std::runtime_error(
            "File table extends beyond archive."
        );
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

    const std::uint64_t path_end =
        checked_add(path_offset, path_length);

    if (path_offset < file_table_end ||
        path_end > archive_size ||
        path_end > header.block_index_offset) {
        throw std::runtime_error(
            "File path extends beyond its valid archive region."
        );
    }

    reader.seekg(path_offset);

    const std::string path = reader.read_string(path_length);

    if (path.empty()) {
        throw std::runtime_error(
            "Archive contains an empty file path."
        );
    }

    const std::uint64_t block_index_size =
        checked_multiply(header.block_count, BLOCK_RECORD_SIZE);

    const std::uint64_t block_index_end =
        checked_add(header.block_index_offset, block_index_size);

    if (block_index_end > archive_size) {
        throw std::runtime_error(
            "Block index extends beyond archive."
        );
    }

    if (header.payload_offset < block_index_end) {
        throw std::runtime_error(
            "Payload overlaps the block index."
        );
    }

    reader.seekg(header.block_index_offset);

    struct BlockMetadata {
        std::uint32_t block_id;
        std::uint64_t payload_offset;
        std::uint64_t original_length;
        std::uint64_t stored_length;
        TransformId transform_id;
        CodecId codec_id;
        std::uint16_t transform_parameter;
        std::uint32_t checksum;
    };

    std::vector<BlockMetadata> metadata;
    metadata.reserve(header.block_count);

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

        const std::uint16_t transform_parameter =
            reader.read_u16();

        const std::uint32_t checksum = reader.read_u32();

        if (block_id != i) {
            throw std::runtime_error(
                "Block IDs are not sequential."
            );
        }

        validate_block(
            block_id,
            block_file_id,
            original_length,
            stored_length,
            transform_id,
            codec_id,
            transform_parameter
        );

        if (stored_length >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max())) {
            throw std::runtime_error(
                "Stored block exceeds supported size limits."
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

        metadata.push_back({
            block_id,
            payload_offset,
            original_length,
            stored_length,
            transform_id,
            codec_id,
            transform_parameter,
            checksum
        });
    }

    std::vector<std::uint8_t> result;

    if (original_size >
        static_cast<std::uint64_t>(result.max_size())) {
        throw std::runtime_error(
            "Original file size exceeds supported limits."
        );
    }

    result.reserve(static_cast<std::size_t>(original_size));

    for (const auto& block : metadata) {
        reader.seekg(block.payload_offset);

        const auto stored_data =
            reader.read_bytes(
                static_cast<std::size_t>(block.stored_length)
            );

        // Raw blocks need no transform or codec decoding.
        std::vector<std::uint8_t> decoded;

        if (block.transform_id == TransformId::None &&
            block.codec_id == CodecId::None) {
            decoded = stored_data;
        } else {
            decoded = Codec::decode(
                stored_data,
                block.codec_id
            );

            decoded = Transform::decode(
                decoded,
                block.transform_id
            );
        }

        if (decoded.size() != block.original_length) {
            throw std::runtime_error(
                "Decoded block length does not match metadata."
            );
        }

        // Checksum is calculated over the reconstructed original bytes.
        if (calculate_checksum(decoded) != block.checksum) {
            throw std::runtime_error(
                "Block checksum mismatch."
            );
        }

        if (decoded.size() > result.max_size() - result.size()) {
            throw std::runtime_error(
                "Reconstructed file exceeds supported size limits."
            );
        }

        result.insert(
            result.end(),
            decoded.begin(),
            decoded.end()
        );
    }

    if (result.size() != original_size) {
        throw std::runtime_error(
            "Reconstructed file size does not match metadata."
        );
    }

    return result;
}

} // namespace uatc
