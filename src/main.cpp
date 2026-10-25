
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "archive/archive_reader.hpp"
#include "archive/archive_writer.hpp"
#include "archive/block.hpp"
#include "filesystem/file_enumerator.hpp"
#include "filesystem/file_reader.hpp"

namespace fs = std::filesystem;

namespace {

constexpr std::size_t BLOCK_SIZE = 64 * 1024;

void write_file(
    const fs::path& output_path,
    const std::vector<std::uint8_t>& data
) {
    if (!output_path.parent_path().empty()) {
        fs::create_directories(output_path.parent_path());
    }

    std::ofstream output(
        output_path,
        std::ios::binary | std::ios::trunc
    );

    if (!output) {
        throw std::runtime_error(
            "Could not create output file: " + output_path.string()
        );
    }

    if (!data.empty()) {
        output.write(
            reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size())
        );
    }

    if (!output) {
        throw std::runtime_error(
            "Failed while writing output file: " + output_path.string()
        );
    }
}

void compress_file(
    const fs::path& input_file,
    const fs::path& archive_file
) {
    const auto data = uatc::FileReader::read(input_file);
    const auto blocks = uatc::BlockSplitter::split(data, BLOCK_SIZE);

    if (!archive_file.parent_path().empty()) {
        fs::create_directories(archive_file.parent_path());
    }

    uatc::ArchiveWriter::write(archive_file, input_file, blocks);

    std::cout << "Compressed: " << input_file
              << " -> " << archive_file << '\n';
}

void extract_file(
    const fs::path& archive_file,
    const fs::path& output_file
) {
    const auto data = uatc::ArchiveReader::read(archive_file);
    write_file(output_file, data);

    std::cout << "Extracted: " << archive_file
              << " -> " << output_file << '\n';
}

void compress_path(
    const fs::path& input_path,
    const fs::path& output_path
) {
    if (!fs::exists(input_path)) {
        throw std::runtime_error(
            "Input path does not exist: " + input_path.string()
        );
    }

    if (fs::is_regular_file(input_path)) {
        compress_file(input_path, output_path);
        return;
    }

    if (!fs::is_directory(input_path)) {
        throw std::runtime_error("Input must be a file or directory.");
    }

    const auto files = uatc::FileEnumerator::enumerate(input_path);

    fs::create_directories(output_path);

    std::size_t count = 0;

    for (const auto& file : files) {
        const fs::path source = input_path / file.relative_path;

        fs::path archive = output_path / file.relative_path;
        archive += ".uatc";

        compress_file(source, archive);
        ++count;
    }

    std::cout << "Directory compression complete. Files: "
              << count << '\n';
}

void extract_path(
    const fs::path& input_path,
    const fs::path& output_path
) {
    if (!fs::exists(input_path)) {
        throw std::runtime_error(
            "Input path does not exist: " + input_path.string()
        );
    }

    if (fs::is_regular_file(input_path)) {
        extract_file(input_path, output_path);
        return;
    }

    if (!fs::is_directory(input_path)) {
        throw std::runtime_error(
            "Input must be an archive file or directory."
        );
    }

    const auto files = uatc::FileEnumerator::enumerate(input_path);
    fs::create_directories(output_path);

    std::size_t count = 0;

    for (const auto& file : files) {
        const fs::path archive = input_path / file.relative_path;

        if (archive.extension() != ".uatc") {
            continue;
        }

        fs::path restored_relative = file.relative_path;
        restored_relative.replace_extension();

        const fs::path destination = output_path / restored_relative;

        extract_file(archive, destination);
        ++count;
    }

    std::cout << "Directory extraction complete. Files: "
              << count << '\n';
}

void print_usage() {
    std::cout
        << "UATC command-line tool\n\n"
        << "Usage:\n"
        << "  uatc_cli compress <input-file-or-directory> <output>\n"
        << "  uatc_cli extract  <archive-file-or-directory> <output>\n\n"
        << "For a file, output is the archive path or restored file path.\n"
        << "For a directory, output is the destination directory.\n";
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc != 4) {
            print_usage();
            return 1;
        }

        const std::string command = argv[1];
        const fs::path input_path = argv[2];
        const fs::path output_path = argv[3];

        if (command == "compress") {
            compress_path(input_path, output_path);
        } else if (command == "extract") {
            extract_path(input_path, output_path);
        } else {
            print_usage();
            return 1;
        }

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
