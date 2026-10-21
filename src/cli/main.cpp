
#include "archive/archive_inspector.hpp"
#include "archive/archive_reader.hpp"
#include "archive/archive_verifier.hpp"
#include "archive/archive_writer.hpp"
#include "archive/block.hpp"
#include "core/compressibility_probe.hpp"
#include "core/compressor.hpp"
#include "core/config.hpp"
#include "core/error.hpp"
#include "core/transform_factory.hpp"
#include "core/transform_selector.hpp"
#include "filesystem/file_reader.hpp"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

void print_help() {
    std::cout
        << "UATC - Universal Adaptive Transform Compression\n\n"
        << "Usage:\n"
        << "  uatc --help\n"
        << "  uatc compress <input> <output.uatc> [--block-size BYTES] [--quiet]\n"
        << "  uatc extract <archive.uatc> <output-dir> [--quiet]\n"
        << "  uatc info <archive.uatc>\n"
        << "  uatc verify <archive.uatc>\n\n"
        << "Commands:\n"
        << "  compress  Compress a file into a .uatc archive.\n"
        << "  extract   Restore the contents of a .uatc archive.\n"
        << "  info      Display archive metadata without decoding payloads.\n"
        << "  verify    Validate archive structure and payload integrity.\n\n"
        << "Options:\n"
        << "  --block-size BYTES   Block size to split the input into "
        << "(default 1 MiB).\n"
        << "  --quiet              Print only errors.\n";
}

// The name comes from the compression core, so the CLI always agrees with the
// transform engine and needs no change when a transform is added.
const char* transform_name(uatc::TransformId id) {
    static thread_local std::string name;
    auto transform = uatc::core::make_transform(id);
    name = transform ? transform->name() : "Unknown";
    return name.c_str();
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

std::size_t parse_size(const std::string& value, const char* option) {
    try {
        const auto parsed = std::stoull(value);
        if (parsed == 0) throw std::invalid_argument("zero");
        return static_cast<std::size_t>(parsed);
    } catch (const std::exception&) {
        throw std::runtime_error(
            std::string("invalid value for ") + option + ": " + value
        );
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

int compress_file(const std::filesystem::path& input,
                  const std::filesystem::path& output,
                  std::size_t block_size,
                  bool quiet) {
    uatc::core::CompressOptions options;
    options.block_size = block_size;

    const auto result =
        uatc::core::CompressionManager::compress(input, output, options);

    if (quiet) {
        return 0;
    }

    std::cout
        << "Compressed:        " << input.string() << '\n'
        << "Archive:           " << output.string() << '\n'
        << "Original bytes:    " << result.originalBytes << '\n'
        << "Archive bytes:     " << result.archiveBytes << '\n'
        << "Blocks:            " << result.blockCount << '\n';

    std::cout << "Ratio:             ";

    if (result.archiveBytes == 0) {
        std::cout << "N/A\n";
    } else {
        std::cout << std::fixed << std::setprecision(2)
                  << result.ratio << '\n';
    }

    std::cout
        << "Stored blocks:     " << result.storedBlocks << '\n'
        << "Compressed blocks: " << result.compressedBlocks << '\n'
        << "Transforms:\n";

    for (const auto& entry : result.transformUsage) {
        std::cout
            << "  " << transform_name(entry.first)
            << " (" << static_cast<unsigned int>(entry.first) << "): "
            << entry.second << " blocks\n";
    }

    return 0;
}

int extract_archive(const std::filesystem::path& archive,
                    const std::filesystem::path& output_dir,
                    bool quiet) {
    const auto result =
        uatc::core::CompressionManager::extract(archive, output_dir);

    if (!quiet) {
        std::cout
            << "Extracted:         " << result.destination.string() << '\n'
            << "Bytes written:     " << result.bytesWritten << '\n';
    }

    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        print_help();
        return 0;
    }

    if (argc < 3) {
        print_help();
        std::cerr
            << "\nerror: E_INVALID_ARGUMENT: expected a command and its arguments.\n";
        return 2;
    }

    const std::string command = argv[1];

    try {
        if (command == "compress") {
            if (argc < 4) {
                print_help();
                std::cerr << "\nerror: E_INVALID_ARGUMENT: "
                          << "compress needs an input and an output path.\n";
                return 2;
            }

            const std::filesystem::path input = argv[2];
            const std::filesystem::path output = argv[3];

            std::size_t block_size = uatc::core::kDefaultBlockSize;
            bool quiet = false;

            for (int i = 4; i < argc; ++i) {
                const std::string option = argv[i];
                if (option == "--quiet") {
                    quiet = true;
                } else if (option == "--block-size") {
                    if (i + 1 >= argc) {
                        std::cerr << "error: E_INVALID_ARGUMENT: "
                                  << "--block-size needs a value.\n";
                        return 2;
                    }
                    try {
                        block_size = parse_size(argv[++i], "--block-size");
                    } catch (const std::exception& error) {
                        std::cerr << "error: E_INVALID_ARGUMENT: "
                                  << error.what() << '\n';
                        return 2;
                    }
                } else {
                    print_help();
                    std::cerr << "\nerror: E_INVALID_ARGUMENT: unknown option: "
                              << option << '\n';
                    return 2;
                }
            }

            return compress_file(input, output, block_size, quiet);
        }

        if (command == "extract") {
            if (argc < 4) {
                print_help();
                std::cerr << "\nerror: E_INVALID_ARGUMENT: "
                          << "extract needs an archive and an output directory.\n";
                return 2;
            }

            const std::filesystem::path archive = argv[2];
            const std::filesystem::path output_dir = argv[3];

            bool quiet = false;
            for (int i = 4; i < argc; ++i) {
                if (std::string(argv[i]) == "--quiet") {
                    quiet = true;
                } else {
                    print_help();
                    std::cerr << "\nerror: E_INVALID_ARGUMENT: unknown option: "
                              << argv[i] << '\n';
                    return 2;
                }
            }

            return extract_archive(archive, output_dir, quiet);
        }

        if (command != "info" && command != "verify") {
            print_help();
            std::cerr << "\nerror: E_INVALID_ARGUMENT: unknown command: "
                      << command << '\n';
            return 2;
        }

        if (argc != 3) {
            print_help();
            std::cerr << "\nerror: E_INVALID_ARGUMENT: "
                      << "expected a command and archive path.\n";
            return 2;
        }

        const std::filesystem::path archive_path = argv[2];

        if (!std::filesystem::exists(archive_path)) {
            std::cerr << "error: E_NOT_FOUND: archive does not exist: "
                      << archive_path.string() << '\n';
            return 3;
        }

        if (!std::filesystem::is_regular_file(archive_path)) {
            std::cerr << "error: E_INVALID_ARGUMENT: "
                      << "archive path is not a file.\n";
            return 2;
        }

        if (command == "info") {
            return print_info(archive_path);
        }

        return print_verify(archive_path);

    } catch (const uatc::core::UatcError& error) {
        std::cerr << "error: " << uatc::core::error_code_name(error.code())
                  << ": " << error.what() << '\n';
        return uatc::core::error_exit_code(error.code());

    } catch (const std::filesystem::filesystem_error& error) {
        std::cerr << "error: E_IO: " << error.what() << '\n';
        return 5;

    } catch (const std::exception& error) {
        std::cerr << "error: E_MALFORMED_ARCHIVE: "
                  << error.what() << '\n';
        return 10;
    }
}
