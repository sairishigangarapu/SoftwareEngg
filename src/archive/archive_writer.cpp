
#include "archive/archive_writer.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "archive/archive_binary.hpp"
#include "archive/codec.hpp"
#include "archive/transforms.hpp"
#include "core/crc32.hpp"

namespace uatc {

namespace {

// CRC-32 of the original block bytes, the checksum the SAD specifies.
std::uint32_t calculate_checksum(
    const std::vector<std::uint8_t>& data
) {
    return core::crc32(data.data(), data.size());
}

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
    std::vector<CodedBlock> coded(
        blocks.size(),
        CodedBlock{}
    );

    for (std::size_t i = 0; i < blocks.size(); ++i) {
        coded[i].transform_id = encodings[i].transform_id;
        coded[i].codec_id = encodings[i].codec_id;
        coded[i].payload = Transform::encode(
            blocks[i].data,
            encodings[i].transform_id
        );
        coded[i].payload = Codec::encode(
            coded[i].payload,
            encodings[i].codec_id
        );
    }

    write(archive_path, source_file, blocks, coded);
}

void ArchiveWriter::write(
    const std::filesystem::path& archive_path,
    const std::filesystem::path& source_file,
    const std::vector<Block>& blocks,
    const std::vector<CodedBlock>& coded
) {
    if (!std::filesystem::exists(source_file)) {
        throw std::runtime_error(
            "Source file does not exist: " +
            source_file.string()
        );
    }

    if (!std::filesystem::is_regular_file(source_file)) {
        throw std::runtime_error(
            "Source path is not a regular file: " +
            source_file.string()
        );
    }

    if (coded.size() != blocks.size()) {
        throw std::invalid_argument(
            "Each block must have one encoding configuration."
        );
    }

    if (blocks.size() >
        static_cast<std::size_t>(
            UINT32_MAX
        )) {
        throw std::invalid_argument(
            "Too many blocks for the archive format."
        );
    }

    const std::string relative_path =
        source_file.filename().generic_string();

    const std::uint64_t original_size =
        std::filesystem::file_size(source_file);

    std::vector<std::uint32_t> checksums;
    checksums.reserve(blocks.size());

    for (const auto& block : blocks) {
        checksums.push_back(
            calculate_checksum(block.data)
        );
    }

    ArchiveHeader header;
    header.magic = UATC_MAGIC;
    header.version = UATC_VERSION;
    header.header_size = UATC_ARCHIVE_HEADER_SIZE;
    header.file_count = 1;
    header.block_count =
        static_cast<std::uint32_t>(blocks.size());

    header.file_table_offset = UATC_ARCHIVE_HEADER_SIZE;

    const std::uint64_t path_offset =
        header.file_table_offset + UATC_FILE_RECORD_SIZE;

    header.block_index_offset =
        path_offset + relative_path.size();

    header.payload_offset =
        header.block_index_offset +
        UATC_BLOCK_RECORD_SIZE *
        static_cast<std::uint64_t>(blocks.size());

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
        blocks.empty() ? 0 : blocks.front().block_id
    );
    writer.write_u32(
        static_cast<std::uint32_t>(blocks.size())
    );

    writer.write_string(relative_path);

    // Block index.
    std::uint64_t current_payload_offset = header.payload_offset;

    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const auto& block = blocks[i];
        const auto& payload = coded[i].payload;
        const auto& encoding = coded[i];

        writer.write_u32(0);
        writer.write_u32(block.block_id);
        writer.write_u64(current_payload_offset);
        writer.write_u64(
            static_cast<std::uint64_t>(block.data.size())
        );
        writer.write_u64(
            static_cast<std::uint64_t>(payload.size())
        );

        writer.write_u8(
            static_cast<std::uint8_t>(encoding.transform_id)
        );
        writer.write_u8(
            static_cast<std::uint8_t>(encoding.codec_id)
        );
        writer.write_u16(0);

        // Store the checksum of the original, unencoded block.
        writer.write_u32(checksums[i]);

        current_payload_offset +=
            static_cast<std::uint64_t>(payload.size());
    }

    // Write encoded block payloads.
    for (const auto& block : coded) {
        writer.write_bytes(block.payload);
    }
}

} // namespace uatc
