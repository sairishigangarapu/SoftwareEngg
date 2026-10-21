
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "archive/archive_format.hpp"

namespace uatc {

// A file recovered from an archive: its recorded relative path and contents.
struct ArchivedFile {
    std::string relative_path;
    std::vector<std::uint8_t> data;
};

class ArchiveReader {
public:
    static std::vector<std::uint8_t> read(
        const std::filesystem::path& archive_path
    );

    // Reads the archive and also reports the relative path it recorded for
    // the file, so the caller can restore it under the original name.
    static ArchivedFile read_file(
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
