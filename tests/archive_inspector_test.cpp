
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "archive/archive_inspector.hpp"
#include "archive/archive_writer.hpp"
#include "archive/archive_reader.hpp"
#include "archive/block.hpp"
#include "filesystem/file_reader.hpp"

int main() {
    namespace fs = std::filesystem;

    const fs::path source = "test_data/hello.txt";
    const fs::path archive = "inspector_test.uatc";

    try {
        const auto original = uatc::FileReader::read(source);
        const auto blocks = uatc::BlockSplitter::split(original, 4);

        uatc::ArchiveWriter::write(archive, source, blocks);

        const auto info = uatc::ArchiveInspector::inspect(archive);

        if (info.fileCount != 1) {
            throw std::runtime_error("Incorrect file count.");
        }

        if (info.blockCount != blocks.size()) {
            throw std::runtime_error("Incorrect block count.");
        }

        if (info.originalBytes != original.size()) {
            throw std::runtime_error("Incorrect original size.");
        }

        if (info.archiveBytes != fs::file_size(archive)) {
            throw std::runtime_error("Incorrect archive size.");
        }

        if (info.formatVersion != uatc::UATC_VERSION) {
            throw std::runtime_error("Incorrect archive version.");
        }

        if (info.storedBlocks != blocks.size()) {
            throw std::runtime_error("Incorrect stored block count.");
        }

        if (info.compressedBlocks != 0) {
            throw std::runtime_error("Unexpected compressed blocks.");
        }

        // Inspection must not alter the archive.
        if (uatc::ArchiveReader::read(archive) != original) {
            throw std::runtime_error(
                "Archive contents changed after inspection."
            );
        }

        fs::remove(archive);

        std::cout << "Archive inspector test passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code ignored;
        fs::remove(archive, ignored);

        std::cerr << "Archive inspector test failed: "
                  << error.what() << '\n';
        return 1;
    }
}
