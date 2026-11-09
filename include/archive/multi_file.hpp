#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "archive/archive_writer.hpp"

namespace uatc {

struct ArchiveInput {
    std::string relative_path;
    std::vector<std::uint8_t> data;
};

struct ArchivedTree {
    struct File {
        std::string relative_path;
        std::uint64_t original_size = 0;
        std::vector<std::uint8_t> data;
    };

    std::vector<File> files;
};

class MultiFileArchiveWriter {
public:
    static void write(
        const std::filesystem::path& archive_path,
        std::vector<ArchiveInput> files,
        std::size_t block_size
    );

    static void write_directory(
        const std::filesystem::path& archive_path,
        const std::filesystem::path& directory,
        std::size_t block_size
    );
};

class MultiFileArchiveReader {
public:
    static ArchivedTree read(const std::filesystem::path& archive_path);

    static void extract(
        const std::filesystem::path& archive_path,
        const std::filesystem::path& output_directory
    );
};

}  // namespace uatc
