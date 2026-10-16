
#pragma once

#include <filesystem>
#include <vector>

#include "archive/block.hpp"
#include "archive/archive_format.hpp"

namespace uatc {

struct BlockEncoding {
    TransformId transform_id = TransformId::None;
    CodecId codec_id = CodecId::None;
};

class ArchiveWriter {
public:
    static void write(
        const std::filesystem::path& archive_path,
        const std::filesystem::path& source_file,
        const std::vector<Block>& blocks
    );

    // Encode every block with the same transform and codec.
    static void write(
        const std::filesystem::path& archive_path,
        const std::filesystem::path& source_file,
        const std::vector<Block>& blocks,
        TransformId transform_id,
        CodecId codec_id
    );

    // Encode each block using its own transform and codec.
    static void write(
        const std::filesystem::path& archive_path,
        const std::filesystem::path& source_file,
        const std::vector<Block>& blocks,
        const std::vector<BlockEncoding>& encodings
    );
};

}
