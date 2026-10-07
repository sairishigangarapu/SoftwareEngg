#pragma once

#include <filesystem>
#include <vector>
#include <cstdint>

namespace uatc {

class FileReader {
public:
    static std::vector<std::uint8_t> read(
        const std::filesystem::path& file_path
    );
};

}