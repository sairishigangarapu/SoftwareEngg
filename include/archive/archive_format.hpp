
#pragma once

#include <cstdint>
#include <string>

namespace uatc {

constexpr std::uint32_t UATC_MAGIC = 0x43544155;
constexpr std::uint16_t UATC_VERSION = 1;

constexpr std::uint16_t UATC_ARCHIVE_HEADER_SIZE = 40;
constexpr std::uint32_t UATC_FILE_RECORD_SIZE = 32;
constexpr std::uint32_t UATC_BLOCK_RECORD_SIZE = 40;

enum class TransformId : std::uint8_t {
    None = 0,
    RLE = 1,
    Delta = 2
};

enum class CodecId : std::uint8_t {
    None = 0,
    Baseline = 1
};

struct ArchiveHeader {
    std::uint32_t magic = UATC_MAGIC;
    std::uint16_t version = UATC_VERSION;
    std::uint16_t header_size = UATC_ARCHIVE_HEADER_SIZE;
    std::uint32_t file_count = 0;
    std::uint32_t block_count = 0;
    std::uint64_t file_table_offset = 0;
    std::uint64_t block_index_offset = 0;
    std::uint64_t payload_offset = 0;
};

struct FileRecord {
    std::uint32_t file_id = 0;
    std::uint64_t path_offset = 0;
    std::uint32_t path_length = 0;
    std::uint64_t original_size = 0;
    std::uint32_t first_block = 0;
    std::uint32_t block_count = 0;
};

struct BlockRecord {
    std::uint32_t file_id = 0;
    std::uint32_t block_id = 0;
    std::uint64_t payload_offset = 0;
    std::uint64_t original_length = 0;
    std::uint64_t stored_length = 0;

    TransformId transform_id = TransformId::None;
    CodecId codec_id = CodecId::None;

    // Serialized in the existing two-byte reserved field.
    std::uint16_t transform_parameter = 0;

    std::uint32_t checksum = 0;
};

}
