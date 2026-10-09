#include "archive/archive_writer.hpp"

#include <stdexcept>
#include <string>

#include "archive/archive_binary.hpp"
#include "archive/transforms.hpp"
#include "archive/codec.hpp"

namespace uatc {

void ArchiveWriter::write(
    const std::filesystem::path& archive_path,
    const std::filesystem::path& source_file,
    const std::vector<Block>& blocks
) {
    write(archive_path, source_file, blocks,
          TransformId::None, CodecId::None);
}

void ArchiveWriter::write(
    const std::filesystem::path& archive_path,
    const std::filesystem::path& source_file,
    const std::vector<Block>& blocks,
    TransformId transform_id,
    CodecId codec_id
) {
    // --------------------------------------------------------
    // Validate source file
    // --------------------------------------------------------

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

    // --------------------------------------------------------
    // Prepare metadata
    // --------------------------------------------------------

    const std::string relative_path =
        source_file.filename().generic_string();

    const std::uint64_t original_size =
        std::filesystem::file_size(source_file);

    std::vector<std::vector<std::uint8_t>> payloads;
    payloads.reserve(blocks.size());
    for (const auto& block : blocks) {
        auto transformed = Transform::encode(block.data, transform_id);
        payloads.push_back(Codec::encode(transformed, codec_id));
    }

    ArchiveHeader header;

    header.magic = UATC_MAGIC;

    header.version = UATC_VERSION;

    header.header_size =
        UATC_ARCHIVE_HEADER_SIZE;

    header.file_count = 1;

    header.block_count =
        static_cast<std::uint32_t>(
            blocks.size()
        );

    // --------------------------------------------------------
    // Calculate archive layout
    //
    // Header
    // File Record
    // File Path
    // Block Index
    // Payload
    // --------------------------------------------------------

    const std::uint64_t header_size =
        UATC_ARCHIVE_HEADER_SIZE;

    const std::uint64_t file_record_size =
        UATC_FILE_RECORD_SIZE;

    const std::uint64_t block_record_size =
        UATC_BLOCK_RECORD_SIZE;

    // --------------------------------------------------------
    // File Table
    // --------------------------------------------------------

    header.file_table_offset =
        header_size;

    // --------------------------------------------------------
    // File Path
    // --------------------------------------------------------

    const std::uint64_t path_offset =
        header.file_table_offset +
        file_record_size;

    // --------------------------------------------------------
    // Block Index
    // --------------------------------------------------------

    const std::uint64_t block_index_offset =
        path_offset +
        static_cast<std::uint64_t>(
            relative_path.size()
        );

    // --------------------------------------------------------
    // Payload
    // --------------------------------------------------------

    const std::uint64_t payload_offset =
        block_index_offset +
        block_record_size *
        static_cast<std::uint64_t>(
            blocks.size()
        );

    header.block_index_offset =
        block_index_offset;

    header.payload_offset =
        payload_offset;

    // --------------------------------------------------------
    // Open archive
    // --------------------------------------------------------

    BinaryWriter writer(archive_path);

    // --------------------------------------------------------
    // Write Header
    // --------------------------------------------------------

    writer.write_u32(
        header.magic
    );

    writer.write_u16(
        header.version
    );

    writer.write_u16(
        header.header_size
    );

    writer.write_u32(
        header.file_count
    );

    writer.write_u32(
        header.block_count
    );

    writer.write_u64(
        header.file_table_offset
    );

    writer.write_u64(
        header.block_index_offset
    );

    writer.write_u64(
        header.payload_offset
    );

    // --------------------------------------------------------
    // Write File Table Record
    // --------------------------------------------------------

    writer.write_u32(0); // file_id

    writer.write_u64(
        path_offset
    );

    writer.write_u32(
        static_cast<std::uint32_t>(
            relative_path.size()
        )
    );

    writer.write_u64(
        original_size
    );

    writer.write_u32(
        blocks.empty()
            ? 0
            : blocks.front().block_id
    );

    writer.write_u32(
        static_cast<std::uint32_t>(
            blocks.size()
        )
    );

    // --------------------------------------------------------
    // Write File Path
    // --------------------------------------------------------

    writer.write_string(
        relative_path
    );

    // --------------------------------------------------------
    // Write Block Index
    // --------------------------------------------------------

    std::uint64_t current_payload_offset =
        payload_offset;

    for (const auto& block : blocks) {

        // File ID
        writer.write_u32(0);

        // Block ID
        writer.write_u32(
            block.block_id
        );

        // Payload offset
        writer.write_u64(
            current_payload_offset
        );

        // Original length
        writer.write_u64(
            static_cast<std::uint64_t>(block.data.size())
        );

        // Encoded payload length
        writer.write_u64(
            static_cast<std::uint64_t>(payloads[&block - blocks.data()].size())
        );

        // Transform and codec IDs
        writer.write_u8(static_cast<std::uint8_t>(transform_id));
        writer.write_u8(static_cast<std::uint8_t>(codec_id));

        // Reserved bytes
        writer.write_u16(0);

        // Checksum placeholder.
        //
        // A real checksum algorithm will be added later.
        writer.write_u32(0);

        current_payload_offset +=
            static_cast<std::uint64_t>(
                payloads[&block - blocks.data()].size()
            );
    }

    // --------------------------------------------------------
    // Write encoded block payloads
    // --------------------------------------------------------

    for (const auto& payload : payloads) {
        writer.write_bytes(payload);
    }
}

}