
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "archive/archive_format.hpp"

namespace uatc {

struct TransformUsage {
    TransformId id = TransformId::None;
    std::string name;
    std::uint64_t blocks = 0;
};

struct CodecUsage {
    CodecId id = CodecId::None;
    std::string name;
    std::uint64_t blocks = 0;
};

struct ArchiveInfo {
    std::filesystem::path path;

    std::uint16_t formatVersion = 0;

    std::uint64_t fileCount = 0;
    std::uint64_t blockCount = 0;
    std::uint64_t originalBytes = 0;
    std::uint64_t archiveBytes = 0;

    double ratio = 0.0;

    std::uint64_t storedBlocks = 0;
    std::uint64_t compressedBlocks = 0;

    std::vector<TransformUsage> transformUsage;
    std::vector<CodecUsage> codecUsage;
};

class ArchiveInspector {
public:
    // Reads archive metadata without decoding block payloads.
    static ArchiveInfo inspect(
        const std::filesystem::path& archive_path
    );
};

}
