#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "archive/archive_format.hpp"
#include "archive/block.hpp"

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
};

}