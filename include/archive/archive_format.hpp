#pragma once

#include <cstdint>
#include <string>

namespace uatc {

// ------------------------------------------------------------
// UATC Archive Format
// ------------------------------------------------------------
//
// All integer fields use:
//   - fixed-width integer types
//   - little-endian byte order
//
// Archive layout:
//
//   [Header]
//   [File Table]
//   [Block Index]
//   [Payload]
//
// ------------------------------------------------------------

constexpr std::uint32_t UATC_MAGIC = 0x43544155; // "UATC"
constexpr std::uint16_t UATC_VERSION = 1;

// ------------------------------------------------------------
// Transform IDs
// ------------------------------------------------------------

enum class TransformId : std::uint8_t {
    None = 0,
    RLE = 1,
    Delta = 2
};

// ------------------------------------------------------------
// Codec IDs
// ------------------------------------------------------------

enum class CodecId : std::uint8_t {
    None = 0,
    Baseline = 1
};

// ------------------------------------------------------------
// Archive Header
// ------------------------------------------------------------

struct ArchiveHeader {
    std::uint32_t magic = UATC_MAGIC;
    std::uint16_t version = UATC_VERSION;
    std::uint16_t header_size = sizeof(ArchiveHeader);

    std::uint32_t file_count = 0;
    std::uint32_t block_count = 0;

    std::uint64_t file_table_offset = 0;
    std::uint64_t block_index_offset = 0;
    std::uint64_t payload_offset = 0;
};

// ------------------------------------------------------------
// File Table Record
// ------------------------------------------------------------

struct FileRecord {
    std::uint32_t file_id = 0;

    std::uint64_t path_offset = 0;
    std::uint32_t path_length = 0;

    std::uint64_t original_size = 0;

    std::uint32_t first_block = 0;
    std::uint32_t block_count = 0;
};

// ------------------------------------------------------------
// Block Index Record
// ------------------------------------------------------------

struct BlockRecord {
    std::uint32_t file_id = 0;
    std::uint32_t block_id = 0;

    std::uint64_t payload_offset = 0;

    std::uint64_t original_length = 0;
    std::uint64_t stored_length = 0;

    TransformId transform_id = TransformId::None;
    CodecId codec_id = CodecId::None;

    std::uint32_t checksum = 0;
};

}