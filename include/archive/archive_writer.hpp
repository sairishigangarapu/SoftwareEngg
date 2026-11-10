
#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "archive/archive_format.hpp"
#include "archive/block.hpp"

namespace uatc {

struct BlockEncoding {
    TransformId transform_id = TransformId::None;
    CodecId codec_id = CodecId::None;
};

struct CodedBlock {
    TransformId transform_id = TransformId::None;
    CodecId codec_id = CodecId::None;
    std::vector<std::uint8_t> payload;
};

class ArchiveWriter {
public:
    static void write(
        const std::filesystem::path& archive_path,
        const std::filesystem::path& source_file,
        const std::vector<Block>& blocks
    );

    static void write(
        const std::filesystem::path& archive_path,
        const std::filesystem::path& source_file,
        const std::vector<Block>& blocks,
        TransformId transform_id,
        CodecId codec_id
    );

    static void write(
        const std::filesystem::path& archive_path,
        const std::filesystem::path& source_file,
        const std::vector<Block>& blocks,
        const std::vector<BlockEncoding>& encodings
    );

    static void write(
        const std::filesystem::path& archive_path,
        const std::filesystem::path& source_file,
        const std::vector<Block>& blocks,
        const std::vector<CodedBlock>& coded
    );
};

}  // namespace uatc
