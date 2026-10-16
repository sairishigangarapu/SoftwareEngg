
#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "archive/archive_format.hpp"

namespace uatc {

class ArchiveReader {
public:
    static std::vector<std::uint8_t> read(
        const std::filesystem::path& archive_path
    );

private:
    static void validate_header(
        const ArchiveHeader& header
    );

    static void validate_block(
        std::uint32_t block_id,
        std::uint32_t file_id,
        std::uint64_t original_length,
        std::uint64_t stored_length,
        TransformId transform_id,
        CodecId codec_id,
        std::uint16_t transform_parameter
    );
};

}
