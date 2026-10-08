#include "archive/archive_writer.hpp"

#include <stdexcept>
#include <string>

#include "archive/archive_binary.hpp"

namespace uatc {

void ArchiveWriter::write(
    const std::filesystem::path& archive_path,
    const std::filesystem::path& source_file,
    const std::vector<Block>& blocks
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

    // --------------------------------------------------------
    // Prepare metadata
    // --------------------------------------------------------

    const std::string relative_path =
        source_file.filename().generic_string();

    const std::uint64_t original_size =
        std::filesystem::file_size(source_file);

    ArchiveHeader header;

    header.magic = UATC_MAGIC;
    header.version = UATC_VERSION;

    header.file_count = 1;

    header.block_count =
        static_cast<std::uint32_t>(blocks.size());

    // --------------------------------------------------------
    // Calculate archive layout
    //
    // Header
    // File Record
    // Path
    // Block Records
    // Payload
    // --------------------------------------------------------

    const std::uint64_t header_size =
        48;

    const std::uint64_t file_record_size =
        36;

    const std::uint64_t block_record_size =
        48;

    header.header_size =
        static_cast<std::uint16_t>(header_size);

    header.file_table_offset =
        header_size;

    const std::uint64_t path_offset =
        header.file_table_offset +
        file_record_size;

    const std::uint64_t block_index_offset =
        path_offset +
        static_cast<std::uint64_t>(
            relative_path.size()
        );

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

    writer.write_u32(header.magic);
    writer.write_u16(header.version);
    writer.write_u16(header.header_size);

    writer.write_u32(header.file_count);
    writer.write_u32(header.block_count);

    writer.write_u64(header.file_table_offset);
    writer.write_u64(header.block_index_offset);
    writer.write_u64(header.payload_offset);

    // --------------------------------------------------------
    // Write File Table Record
    // --------------------------------------------------------

    writer.write_u32(0); // file_id

    writer.write_u64(path_offset);

    writer.write_u32(
        static_cast<std::uint32_t>(
            relative_path.size()
        )
    );

    writer.write_u64(original_size);

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

    writer.write_string(relative_path);

    // --------------------------------------------------------
    // Write Block Index
    // --------------------------------------------------------

    std::uint64_t current_payload_offset =
        payload_offset;

    for (const auto& block : blocks) {
        writer.write_u32(0); // file_id

        writer.write_u32(block.block_id);

        writer.write_u64(current_payload_offset);

        writer.write_u64(
            static_cast<std::uint64_t>(
                block.data.size()
            )
        );

        writer.write_u64(
            static_cast<std::uint64_t>(
                block.data.size()
            )
        );

        // Raw block
        writer.write_u8(
            static_cast<std::uint8_t>(
                TransformId::None
            )
        );

        writer.write_u8(
            static_cast<std::uint8_t>(
                CodecId::None
            )
        );

        // Reserved bytes
        writer.write_u16(0);

        // Simple checksum placeholder.
        // A real checksum algorithm will be added later.
        writer.write_u32(0);

        current_payload_offset +=
            static_cast<std::uint64_t>(
                block.data.size()
            );
    }

    // --------------------------------------------------------
    // Write raw block payloads
    // --------------------------------------------------------

    for (const auto& block : blocks) {
        writer.write_bytes(block.data);
    }
}

}