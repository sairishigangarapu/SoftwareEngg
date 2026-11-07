#pragma once

// DAY 20 - Multi-file archives (FR-01/02/03).
//
// The block record already carries a file_id; the header already carries a
// file_count. This is the archive layer's support for archives that hold
// more than one file, so a directory tree compresses into one .uatc.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "archive/archive_reader.hpp"
#include "archive/archive_writer.hpp"

namespace uatc {

// One input file for a multi-file archive.
struct ArchiveInput {
    std::string relative_path;      // recorded in the file table
    std::vector<std::uint8_t> data; // the file's bytes
};

class MultiFileArchiveWriter {
 public:
    // Encodes every block of every file with the adaptive engine's choices
    // already made (the core decides; the archive layer serialises).
    static void write(const std::filesystem::path& archive_path,
                      const std::vector<ArchiveInput>& files,
                      const std::vector<std::vector<CodedBlock>>& coded,
                      std::size_t block_size);
};

// The reader's multi-file result: every recorded file, in file-table order.
struct ArchivedTree {
    struct File {
        std::string relative_path;
        std::uint64_t original_size = 0;
        std::vector<std::uint8_t> data;
    };
    std::vector<File> files;
};

class MultiFileArchiveReader {
 public:
    // Reads the whole archive. Verifies the integrity region and every
    // block checksum, exactly like the single-file reader.
    static ArchivedTree read(const std::filesystem::path& archive_path);
};

}  // namespace uatc
