#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "archive/archive_reader.hpp"
#include "archive/archive_writer.hpp"
#include "archive/block.hpp"
#include "filesystem/file_reader.hpp"

namespace fs = std::filesystem;

int main() {
    const fs::path source = "test_data/hello.txt";
    const fs::path archive = "build/integration_test.uatc";
    const fs::path extracted = "build/integration_extracted.bin";

    try {
        const auto original = uatc::FileReader::read(source);
        const auto blocks = uatc::BlockSplitter::split(original, 4);

        uatc::ArchiveWriter::write(archive, source, blocks);
        const auto reconstructed = uatc::ArchiveReader::read(archive);

        std::ofstream output(extracted, std::ios::binary);
        if (!output) {
            throw std::runtime_error("Could not create extracted file.");
        }

        if (!reconstructed.empty()) {
            output.write(
                reinterpret_cast<const char*>(reconstructed.data()),
                static_cast<std::streamsize>(reconstructed.size())
            );
        }
        output.close();

        if (!output) {
            throw std::runtime_error("Failed to write extracted file.");
        }

        std::cout << "Source bytes: " << original.size() << '\n';
        std::cout << "Reconstructed bytes: "
                  << reconstructed.size() << '\n';
        std::cout << "Archive: " << archive.string() << '\n';
        std::cout << "Extracted file: " << extracted.string() << '\n';

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "INTEGRATION TEST FAILED: "
                  << e.what() << '\n';
        return 1;
    }
}
