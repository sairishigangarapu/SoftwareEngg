#pragma once

#include <filesystem>
#include <vector>

#include "archive/block.hpp"
#include "archive/archive_format.hpp"

namespace uatc {

class ArchiveWriter {
public:
    static void write(
        const std::filesystem::path& archive_path,
        const std::filesystem::path& source_file,
        const std::vector<Block>& blocks
    );
};

}