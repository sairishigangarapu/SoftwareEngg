#include "filesystem/file_reader.hpp"

#include <cerrno>
#include <fstream>
#include <stdexcept>
#include <system_error>

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

    errno = 0;
    std::ifstream file(file_path, std::ios::binary);

    if (!file) {
        // Keep the operating system's reason, so a file the user may not
        // read is reported as E_PERMISSION rather than a generic disk error
        // (STP TC-FS-03; classify_exception maps EACCES/EPERM).
        const int reason = errno;
        if (reason != 0) {
            throw std::filesystem::filesystem_error(
                "Could not open file", file_path,
                std::error_code(reason, std::generic_category()));
        }
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