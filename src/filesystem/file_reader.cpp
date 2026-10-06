#include "filesystem/file_reader.hpp"

#include <fstream>
#include <stdexcept>

namespace uatc {

std::vector<std::uint8_t> FileReader::read(
    const std::filesystem::path& file_path
) {
    if (!std::filesystem::exists(file_path)) {
        throw std::runtime_error("File does not exist: " + file_path.string());
    }

    if (!std::filesystem::is_regular_file(file_path)) {
        throw std::runtime_error("Path is not a regular file: " + file_path.string());
    }

    std::ifstream file(file_path, std::ios::binary);

    if (!file) {
        throw std::runtime_error("Could not open file: " + file_path.string());
    }

    file.seekg(0, std::ios::end);
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size < 0) {
        throw std::runtime_error("Could not determine file size.");
    }

    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));

    if (size > 0) {
        file.read(
            reinterpret_cast<char*>(data.data()),
            size
        );

        if (!file) {
            throw std::runtime_error("Failed to read file.");
        }
    }

    return data;
}

}