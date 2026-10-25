
#include "archive/archive_writer.hpp"

#include <algorithm>
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

// FNV-1a 32-bit checksum of the original block bytes.
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

struct PreparedBlock {
    Block block;
    BlockEncoding encoding;
    std::vector<std::uint8_t> payload;
    std::uint32_t checksum = 0;
};

} // namespace

void ArchiveWriter::write(
    const std::filesystem::path& archive_path,
    const std::filesystem::path& source_file,
    const std::vector<Block>& blocks
) {
    write(
        archive_path,
        source_file,
        blocks,
        TransformId::None,
        CodecId::None
    );
}

void ArchiveWriter::write(
    const std::filesystem::path& archive_path,
    const std::filesystem::path& source_file,
    const std::vector<Block>& blocks,
    TransformId transform_id,
    CodecId codec_id
) {
    std::vector<BlockEncoding> encodings(
        blocks.size(),
        BlockEncoding{transform_id, codec_id}
    );

    write(archive_path, source_file, blocks, encodings);
}

void ArchiveWriter::write(
    const std::filesystem::path& archive_path,
    const std::filesystem::path& source_file,
    const std::vector<Block>& blocks,
    const std::vector<BlockEncoding>& encodings
) {
    if (!std::filesystem::exists(source_file)) {
        throw std::runtime_error(
            "Source file does not exist: " + source_file.string()
        );
    }

    if (!std::filesystem::is_regular_file(source_file)) {
        throw std::runtime_error(
            "Source path is not a regular file: " + source_file.string()
        );
    }

    if (encodings.size() != blocks.size()) {
        throw std::invalid_argument(
            "Each block must have one encoding configuration."
        );
    }

    if (blocks.size() > static_cast<std::size_t>(UINT32_MAX)) {
        throw std::invalid_argument(
            "Too many blocks for the archive format."
        );
    }

    const std::string relative_path =
        source_file.filename().generic_string();

    if (relative_path.empty() ||
        relative_path.size() > UINT32_MAX) {
        throw std::invalid_argument(
            "Source filename is empty or too long."
        );
    }

    // Pair every block with its encoding before sorting.
    // This keeps transform and codec metadata attached to the
    // correct block, even if workers finish out of order.
    std::vector<PreparedBlock> prepared;
    prepared.reserve(blocks.size());

    for (std::size_t i = 0; i < blocks.size(); ++i) {
        PreparedBlock item;
        item.block = blocks[i];
        item.encoding = encodings[i];
        prepared.push_back(std::move(item));
    }

    // Deterministic archive order: ascending block ID.
    std::sort(
        prepared.begin(),
        prepared.end(),
        [](const PreparedBlock& a, const PreparedBlock& b) {
            return a.block.block_id < b.block.block_id;
        }
    );

    // The current version-1 reader expects block IDs 0, 1, 2, ...
    for (std::size_t i = 0; i < prepared.size(); ++i) {
        if (prepared[i].block.block_id != i) {
            throw std::invalid_argument(
                "Block IDs must be unique and sequential from zero."
            );
        }
    }

    const std::uint64_t original_size =
        std::filesystem::file_size(source_file);

    std::uint64_t total_original_size = 0;

    for (auto& item : prepared) {
        if (item.block.data.size() >
            std::numeric_limits<std::uint64_t>::max() -
                total_original_size) {
            throw std::invalid_argument(
                "Total block size exceeds archive format limits."
            );
        }

        total_original_size +=
            static_cast<std::uint64_t>(item.block.data.size());

        item.checksum = calculate_checksum(item.block.data);

        const auto transformed = Transform::encode(
            item.block.data,
            item.encoding.transform_id
        );

        item.payload = Codec::encode(
            transformed,
            item.encoding.codec_id
        );
    }

    if (total_original_size != original_size) {
        throw std::invalid_argument(
            "Total block size does not match source file size."
        );
    }

    ArchiveHeader header;
    header.magic = UATC_MAGIC;
    header.version = UATC_VERSION;
    header.header_size = UATC_ARCHIVE_HEADER_SIZE;
    header.file_count = 1;
    header.block_count =
        static_cast<std::uint32_t>(prepared.size());

    header.file_table_offset = UATC_ARCHIVE_HEADER_SIZE;

    const std::uint64_t path_offset =
        header.file_table_offset + UATC_FILE_RECORD_SIZE;

    header.block_index_offset =
        path_offset + static_cast<std::uint64_t>(relative_path.size());

    header.payload_offset =
        header.block_index_offset +
        UATC_BLOCK_RECORD_SIZE *
            static_cast<std::uint64_t>(prepared.size());

    BinaryWriter writer(archive_path);

    // Archive header.
    writer.write_u32(header.magic);
    writer.write_u16(header.version);
    writer.write_u16(header.header_size);
    writer.write_u32(header.file_count);
    writer.write_u32(header.block_count);
    writer.write_u64(header.file_table_offset);
    writer.write_u64(header.block_index_offset);
    writer.write_u64(header.payload_offset);

    // File record.
    writer.write_u32(0);
    writer.write_u64(path_offset);
    writer.write_u32(
        static_cast<std::uint32_t>(relative_path.size())
    );
    writer.write_u64(original_size);
    writer.write_u32(
        prepared.empty() ? 0 : prepared.front().block.block_id
    );
    writer.write_u32(
        static_cast<std::uint32_t>(prepared.size())
    );

    writer.write_string(relative_path);

    // Block index in deterministic block-ID order.
    std::uint64_t current_payload_offset = header.payload_offset;

    for (const auto& item : prepared) {
        writer.write_u32(0);
        writer.write_u32(item.block.block_id);
        writer.write_u64(current_payload_offset);
        writer.write_u64(
            static_cast<std::uint64_t>(item.block.data.size())
        );
        writer.write_u64(
            static_cast<std::uint64_t>(item.payload.size())
        );

        writer.write_u8(
            static_cast<std::uint8_t>(item.encoding.transform_id)
        );
        writer.write_u8(
            static_cast<std::uint8_t>(item.encoding.codec_id)
        );
        writer.write_u16(0);
        writer.write_u32(item.checksum);

        if (item.payload.size() >
            std::numeric_limits<std::uint64_t>::max() -
                current_payload_offset) {
            throw std::invalid_argument(
                "Archive payload offset exceeds format limits."
            );
        }

        current_payload_offset +=
            static_cast<std::uint64_t>(item.payload.size());
    }

    // Payloads are written in the same deterministic order as the index.
    for (const auto& item : prepared) {
        writer.write_bytes(item.payload);
    }
}

} // namespace uatc
