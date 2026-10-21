
#include "archive/archive_inspector.hpp"
#include "archive/archive_verifier.hpp"

#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

void print_help() {
    std::cout
        << "UATC - Universal Adaptive Transform Compression\n\n"
        << "Usage:\n"
        << "  uatc --help\n"
        << "  uatc info <archive.uatc>\n"
        << "  uatc verify <archive.uatc>\n\n"
        << "Commands:\n"
        << "  info    Display archive metadata without decoding payloads.\n"
        << "  verify  Validate archive structure and payload integrity.\n";
}

const char* transform_name(uatc::TransformId id) {
    switch (id) {
        case uatc::TransformId::None:
            return "None";
        case uatc::TransformId::RLE:
            return "RLE";
        case uatc::TransformId::Delta:
            return "Delta";
        default:
            return "Unknown";
    }
}

const char* codec_name(uatc::CodecId id) {
    switch (id) {
        case uatc::CodecId::None:
            return "None";
        case uatc::CodecId::Baseline:
            return "Baseline";
        default:
            return "Unknown";
    }
}

int print_info(const std::filesystem::path& path) {
    const auto info = uatc::ArchiveInspector::inspect(path);

    std::cout
        << "Archive:           " << info.path.string() << '\n'
        << "Format version:    " << info.formatVersion << ".0\n"
        << "Files:             " << info.fileCount << '\n'
        << "Blocks:            " << info.blockCount << '\n'
        << "Original bytes:    " << info.originalBytes << '\n'
        << "Archive bytes:     " << info.archiveBytes << '\n';

    std::cout << "Ratio:             ";

    if (info.archiveBytes == 0) {
        std::cout << "N/A\n";
    } else {
        std::cout << std::fixed << std::setprecision(2)
                  << static_cast<double>(info.originalBytes) /
                     static_cast<double>(info.archiveBytes)
                  << '\n';
    }

    std::cout
        << "Stored blocks:     " << info.storedBlocks << '\n'
        << "Compressed blocks: " << info.compressedBlocks << '\n';

    std::cout << "Transforms:\n";

    for (const auto& usage : info.transformUsage) {
        std::cout
            << "  " << transform_name(usage.id)
            << " (" << static_cast<unsigned int>(usage.id) << "): "
            << usage.blocks << " blocks\n";
    }

    std::cout << "Codecs:\n";

    for (const auto& usage : info.codecUsage) {
        std::cout
            << "  " << codec_name(usage.id)
            << " (" << static_cast<unsigned int>(usage.id) << "): "
            << usage.blocks << " blocks\n";
    }

    return 0;
}

int print_verify(const std::filesystem::path& path) {
    const auto result = uatc::ArchiveVerifier::verify(path);

    if (!result.success) {
        if (result.message == "Block checksum mismatch.") {
            std::cerr << "error: E_BLOCK_CHECKSUM: "
                      << result.message << '\n';
            return 13;
        }

        if (result.message == "Unsupported UATC archive version.") {
            std::cerr << "error: E_UNSUPPORTED_VERSION: "
                      << result.message << '\n';
            return 11;
        }

        std::cerr << "error: E_MALFORMED_ARCHIVE: "
                  << result.message << '\n';
        return 10;
    }

    std::cout
        << "OK: " << result.filesChecked << " file(s), "
        << result.blocksChecked << " block(s) verified.\n";

    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        print_help();
        return 0;
    }

    if (argc != 3) {
        print_help();
        std::cerr
            << "\nerror: E_INVALID_ARGUMENT: "
            << "expected a command and archive path.\n";
        return 2;
    }

    const std::string command = argv[1];
    const std::filesystem::path archive_path = argv[2];

    if (command != "info" && command != "verify") {
        print_help();
        std::cerr
            << "\nerror: E_INVALID_ARGUMENT: unknown command: "
            << command << '\n';
        return 2;
    }

    try {
        if (!std::filesystem::exists(archive_path)) {
            std::cerr
                << "error: E_NOT_FOUND: archive does not exist: "
                << archive_path.string() << '\n';
            return 3;
        }

        if (!std::filesystem::is_regular_file(archive_path)) {
            std::cerr
                << "error: E_INVALID_ARGUMENT: "
                << "archive path is not a file.\n";
            return 2;
        }

        if (command == "info") {
            return print_info(archive_path);
        }

        return print_verify(archive_path);

    } catch (const std::filesystem::filesystem_error& error) {
        std::cerr << "error: E_IO: " << error.what() << '\n';
        return 5;

    } catch (const std::exception& error) {
        std::cerr << "error: E_MALFORMED_ARCHIVE: "
                  << error.what() << '\n';
        return 10;
    }
}
