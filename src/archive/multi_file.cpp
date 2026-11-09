#include "archive/multi_file.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "archive/archive_binary.hpp"
#include "archive/block.hpp"
#include "archive/codec.hpp"
#include "archive/transforms.hpp"
#include "core/compressibility_probe.hpp"
#include "core/crc32.hpp"
#include "core/error.hpp"
#include "core/planner.hpp"
#include "core/sha256.hpp"
#include "core/transform_factory.hpp"
#include "filesystem/file_enumerator.hpp"
#include "filesystem/file_reader.hpp"

namespace uatc {
namespace {

constexpr std::uint64_t kHeaderSize = UATC_ARCHIVE_HEADER_SIZE;
constexpr std::uint64_t kFileRecordSize = UATC_FILE_RECORD_SIZE;
constexpr std::uint64_t kBlockRecordSize = UATC_BLOCK_RECORD_SIZE;
constexpr std::uint64_t kIntegritySize = UATC_INTEGRITY_REGION_SIZE;
constexpr std::uint64_t kFooterSize = UATC_ARCHIVE_FOOTER_SIZE;

[[noreturn]] void malformed(const std::string& message) {
    throw core::UatcError(core::ErrorCode::MalformedArchive, message);
}

std::uint64_t checked_add(std::uint64_t a, std::uint64_t b) {
    if (b > std::numeric_limits<std::uint64_t>::max() - a) {
        malformed("Archive offset overflow.");
    }
    return a + b;
}

std::uint64_t checked_multiply(std::uint64_t a, std::uint64_t b) {
    if (a != 0 && b > std::numeric_limits<std::uint64_t>::max() / a) {
        malformed("Archive size overflow.");
    }
    return a * b;
}

void append_u8(std::vector<std::uint8_t>& out, std::uint8_t value) {
    out.push_back(value);
}

void append_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    for (unsigned int i = 0; i < 2; ++i) {
        out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
    }
}

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (unsigned int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
    }
}

void append_u64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (unsigned int i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
    }
}

std::uint16_t read_u16(const std::vector<std::uint8_t>& bytes,
                       std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset]) |
           (static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& bytes,
                       std::size_t offset) {
    std::uint32_t value = 0;
    for (unsigned int i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(bytes[offset + i]) << (8 * i);
    }
    return value;
}

std::uint64_t read_u64(const std::vector<std::uint8_t>& bytes,
                       std::size_t offset) {
    std::uint64_t value = 0;
    for (unsigned int i = 0; i < 8; ++i) {
        value |= static_cast<std::uint64_t>(bytes[offset + i]) << (8 * i);
    }
    return value;
}

std::string safe_archive_path(const std::string& recorded_path) {
    if (recorded_path.empty() ||
        recorded_path.find('\0') != std::string::npos) {
        throw core::UatcError(
            core::ErrorCode::PathTraversal,
            "The archive contains an empty or invalid file path."
        );
    }

    std::string normalized = recorded_path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    if (normalized.front() == '/' ||
        (normalized.size() >= 2 &&
         std::isalpha(static_cast<unsigned char>(normalized[0])) &&
         normalized[1] == ':')) {
        throw core::UatcError(
            core::ErrorCode::PathTraversal,
            "The archive contains an absolute file path: " + recorded_path
        );
    }

    std::size_t start = 0;
    while (start < normalized.size()) {
        const std::size_t end = normalized.find('/', start);
        const std::size_t length =
            end == std::string::npos ? normalized.size() - start
                                     : end - start;
        const std::string component = normalized.substr(start, length);

        if (component.empty() || component == "." || component == ".." ||
            component.find(':') != std::string::npos) {
            throw core::UatcError(
                core::ErrorCode::PathTraversal,
                "The archive contains an unsafe file path: " + recorded_path
            );
        }

        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    if (normalized.back() == '/') {
        throw core::UatcError(
            core::ErrorCode::PathTraversal,
            "The archive contains a directory path where a file was expected."
        );
    }

    return normalized;
}

std::vector<std::string> path_components(const std::string& path) {
    std::vector<std::string> components;
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t end = path.find('/', start);
        if (end == std::string::npos) {
            components.push_back(path.substr(start));
            break;
        }
        components.push_back(path.substr(start, end - start));
        start = end + 1;
    }
    return components;
}

std::string portable_path_key(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        }
    );
    return value;
}

bool is_within_directory(
    const std::filesystem::path& directory,
    const std::filesystem::path& candidate
) {
    auto directory_part = directory.begin();
    auto candidate_part = candidate.begin();
    for (; directory_part != directory.end(); ++directory_part, ++candidate_part) {
        if (candidate_part == candidate.end() ||
            portable_path_key(directory_part->generic_string()) !=
                portable_path_key(candidate_part->generic_string())) {
            return false;
        }
    }
    return true;
}

void validate_path_set(const std::vector<std::string>& paths) {
    std::set<std::string> portable_paths;
    for (const auto& path : paths) {
        if (!portable_paths.insert(portable_path_key(path)).second) {
            malformed("Archive contains duplicate file paths.");
        }
    }

    for (const auto& path : paths) {
        const std::string key = portable_path_key(path);
        std::size_t slash = path.find('/');
        while (slash != std::string::npos) {
            if (portable_paths.find(key.substr(0, slash)) !=
                portable_paths.end()) {
                malformed("Archive file path conflicts with a parent file.");
            }
            slash = path.find('/', slash + 1);
        }
    }
}

std::vector<std::uint8_t> read_archive(
    const std::filesystem::path& archive_path
) {
    if (!std::filesystem::exists(archive_path) ||
        !std::filesystem::is_regular_file(archive_path)) {
        throw std::runtime_error(
            "Archive is not a regular file: " + archive_path.string()
        );
    }

    const std::uintmax_t size = std::filesystem::file_size(archive_path);
    std::vector<std::uint8_t> bytes;
    if (size > bytes.max_size() ||
        size > static_cast<std::uintmax_t>(
                   std::numeric_limits<std::streamsize>::max()
               )) {
        malformed("Archive exceeds supported size limits.");
    }
    bytes.resize(static_cast<std::size_t>(size));

    std::ifstream input(archive_path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "Cannot open archive: " + archive_path.string()
        );
    }
    if (!bytes.empty()) {
        input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size())
        );
    }
    if (!input || input.peek() != std::ifstream::traits_type::eof()) {
        throw std::runtime_error(
            "Failed to read the complete archive: " + archive_path.string()
        );
    }
    return bytes;
}

struct FileLayout {
    std::uint64_t original_size = 0;
    std::uint32_t first_block = 0;
    std::uint32_t block_count = 0;
    std::string path;
    std::vector<std::uint8_t> data;
};

struct BlockLayout {
    std::uint32_t file_id = 0;
    std::uint32_t block_id = 0;
    std::uint64_t payload_offset = 0;
    std::uint64_t original_length = 0;
    std::uint64_t stored_length = 0;
    TransformId transform_id = TransformId::None;
    CodecId codec_id = CodecId::None;
    std::uint16_t transform_parameter = 0;
    std::uint32_t checksum = 0;
};

void encode_file(
    const ArchiveInput& file,
    std::size_t block_size,
    const core::TransformSelector& selector,
    const core::CompressibilityProbe& probe,
    std::vector<CodedBlock>& coded
) {
    const auto blocks = BlockSplitter::split(file.data, block_size);
    if (blocks.size() > UINT32_MAX) {
        throw std::invalid_argument("Too many blocks in one file.");
    }

    coded.reserve(blocks.size());
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const auto& block = blocks[i];
        const auto plan = core::plan_block(
            core::ByteSpan(block.data),
            i,
            block.offset,
            selector,
            probe
        );

        CodedBlock item;
        item.transform_id = plan.transformId;
        item.codec_id = plan.codecId;
        if (plan.stored) {
            item.payload = block.data;
        } else {
            item.payload = Transform::encode(block.data, plan.transformId);
            item.payload = Codec::encode(item.payload, plan.codecId);
        }
        coded.push_back(std::move(item));
    }
}

bool has_existing_entry(const std::filesystem::path& path) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory) {
        return false;
    }
    if (error) {
        throw std::filesystem::filesystem_error(
            "Cannot inspect extraction path", path, error
        );
    }
    return status.type() != std::filesystem::file_type::not_found;
}

void preflight_destination(
    const std::filesystem::path& root,
    const std::vector<ArchivedTree::File>& files
) {
    if (has_existing_entry(root)) {
        const auto root_status = std::filesystem::symlink_status(root);
        if (std::filesystem::is_symlink(root_status) ||
            !std::filesystem::is_directory(root_status)) {
            throw std::runtime_error(
                "Extraction destination is not a safe directory: " +
                root.string()
            );
        }
    }

    for (const auto& file : files) {
        std::filesystem::path current = root;
        const auto components = path_components(file.relative_path);
        for (std::size_t i = 0; i < components.size(); ++i) {
            current /= components[i];
            if (!has_existing_entry(current)) {
                continue;
            }

            const auto status = std::filesystem::symlink_status(current);
            if (std::filesystem::is_symlink(status)) {
                throw std::runtime_error(
                    "Extraction path contains a symbolic link: " +
                    current.string()
                );
            }

            const bool is_file = i + 1 == components.size();
            if (is_file || !std::filesystem::is_directory(status)) {
                throw std::runtime_error(
                    "Extraction would overwrite an existing path: " +
                    current.string()
                );
            }
        }
    }
}

void remove_created_paths(
    const std::vector<std::filesystem::path>& files,
    const std::vector<std::filesystem::path>& directories
) noexcept {
    std::error_code ignored;
    for (auto it = files.rbegin(); it != files.rend(); ++it) {
        std::filesystem::remove(*it, ignored);
        ignored.clear();
    }
    for (auto it = directories.rbegin(); it != directories.rend(); ++it) {
        std::filesystem::remove(*it, ignored);
        ignored.clear();
    }
}

}  // namespace

void MultiFileArchiveWriter::write(
    const std::filesystem::path& archive_path,
    std::vector<ArchiveInput> files,
    std::size_t block_size
) {
    if (block_size == 0) {
        throw std::invalid_argument("Block size must be greater than zero.");
    }
    if (files.size() > UINT32_MAX) {
        throw std::invalid_argument("Too many files for the archive format.");
    }

    for (auto& file : files) {
        file.relative_path = safe_archive_path(file.relative_path);
        if (file.relative_path.size() > UINT32_MAX) {
            throw std::invalid_argument("Archive path is too long.");
        }
    }
    std::sort(
        files.begin(),
        files.end(),
        [](const ArchiveInput& left, const ArchiveInput& right) {
            return left.relative_path < right.relative_path;
        }
    );

    std::vector<std::string> paths;
    paths.reserve(files.size());
    for (const auto& file : files) {
        paths.push_back(file.relative_path);
    }
    validate_path_set(paths);

    const auto registry = core::make_default_registry();
    const core::TransformSelector selector(registry);
    const core::CompressibilityProbe probe;

    std::vector<std::vector<CodedBlock>> coded(files.size());
    std::uint64_t total_blocks = 0;
    std::uint64_t total_payload_size = 0;
    std::uint64_t total_path_bytes = 0;
    for (std::size_t i = 0; i < files.size(); ++i) {
        encode_file(files[i], block_size, selector, probe, coded[i]);
        if (coded[i].size() >
            std::numeric_limits<std::uint32_t>::max() - total_blocks) {
            throw std::invalid_argument("Too many blocks for the archive format.");
        }
        total_blocks += coded[i].size();
        total_path_bytes = checked_add(
            total_path_bytes,
            static_cast<std::uint64_t>(files[i].relative_path.size()) + 1
        );
        for (const auto& block : coded[i]) {
            total_payload_size = checked_add(
                total_payload_size,
                static_cast<std::uint64_t>(block.payload.size())
            );
        }
    }

    const std::uint64_t file_table_offset = kHeaderSize;
    const std::uint64_t path_table_offset = checked_add(
        file_table_offset,
        checked_multiply(files.size(), kFileRecordSize)
    );
    const std::uint64_t block_index_offset =
        checked_add(path_table_offset, total_path_bytes);
    const std::uint64_t payload_offset = checked_add(
        block_index_offset,
        checked_multiply(total_blocks, kBlockRecordSize)
    );
    const std::uint64_t data_region_size =
        checked_add(payload_offset, total_payload_size);
    const std::uint64_t archive_size =
        checked_add(data_region_size, kIntegritySize);
    std::vector<std::uint8_t> image;
    if (archive_size > image.max_size()) {
        throw std::invalid_argument("Archive exceeds supported size limits.");
    }
    image.reserve(static_cast<std::size_t>(archive_size));

    append_u32(image, UATC_MAGIC);
    append_u16(image, UATC_VERSION);
    append_u16(image, static_cast<std::uint16_t>(kHeaderSize));
    append_u32(image, static_cast<std::uint32_t>(files.size()));
    append_u32(image, static_cast<std::uint32_t>(total_blocks));
    append_u64(image, file_table_offset);
    append_u64(image, block_index_offset);
    append_u64(image, payload_offset);

    std::uint64_t next_path_offset = path_table_offset;
    std::uint32_t next_first_block = 0;
    for (std::size_t i = 0; i < files.size(); ++i) {
        append_u32(image, static_cast<std::uint32_t>(i));
        append_u64(image, next_path_offset);
        append_u32(
            image,
            static_cast<std::uint32_t>(files[i].relative_path.size())
        );
        append_u64(image, static_cast<std::uint64_t>(files[i].data.size()));
        append_u32(image, next_first_block);
        append_u32(image, static_cast<std::uint32_t>(coded[i].size()));

        next_path_offset = checked_add(
            next_path_offset,
            static_cast<std::uint64_t>(files[i].relative_path.size()) + 1
        );
        next_first_block += static_cast<std::uint32_t>(coded[i].size());
    }

    for (const auto& file : files) {
        image.insert(
            image.end(),
            file.relative_path.begin(),
            file.relative_path.end()
        );
        append_u8(image, 0);
    }

    std::uint64_t next_payload_offset = payload_offset;
    for (std::size_t file_id = 0; file_id < files.size(); ++file_id) {
        std::size_t original_offset = 0;
        for (std::size_t block_id = 0; block_id < coded[file_id].size();
             ++block_id) {
            const std::size_t original_length = std::min(
                block_size,
                files[file_id].data.size() - original_offset
            );
            const auto* original =
                files[file_id].data.data() + original_offset;
            const auto& encoded = coded[file_id][block_id];
            append_u32(image, static_cast<std::uint32_t>(file_id));
            append_u32(image, static_cast<std::uint32_t>(block_id));
            append_u64(image, next_payload_offset);
            append_u64(image, static_cast<std::uint64_t>(original_length));
            append_u64(image, static_cast<std::uint64_t>(encoded.payload.size()));
            append_u8(image, static_cast<std::uint8_t>(encoded.transform_id));
            append_u8(image, static_cast<std::uint8_t>(encoded.codec_id));
            append_u16(image, 0);
            append_u32(image, core::crc32(original, original_length));
            original_offset += original_length;
            next_payload_offset = checked_add(
                next_payload_offset,
                static_cast<std::uint64_t>(encoded.payload.size())
            );
        }
    }

    for (const auto& per_file : coded) {
        for (const auto& block : per_file) {
            image.insert(image.end(), block.payload.begin(), block.payload.end());
        }
    }

    if (image.size() != data_region_size) {
        throw std::logic_error("Internal archive layout size mismatch.");
    }

    auto file_hash = core::sha256_begin();
    for (const auto& file : files) {
        core::sha256_feed(file_hash, file.data.data(), file.data.size());
    }
    const auto file_digest = core::sha256_end(file_hash);
    const auto archive_digest = core::sha256(image.data(), image.size());
    image.insert(image.end(), file_digest.begin(), file_digest.end());
    image.insert(image.end(), archive_digest.begin(), archive_digest.end());
    append_u64(image, 0);
    append_u32(image, UATC_MAGIC);

    if (image.size() > static_cast<std::size_t>(
                           std::numeric_limits<std::streamsize>::max()
                       )) {
        throw std::invalid_argument("Archive exceeds supported stream size.");
    }
    std::ofstream output(archive_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error(
            "Cannot create archive: " + archive_path.string()
        );
    }
    if (!image.empty()) {
        output.write(
            reinterpret_cast<const char*>(image.data()),
            static_cast<std::streamsize>(image.size())
        );
    }
    output.close();
    if (!output) {
        throw std::runtime_error(
            "Failed while writing archive: " + archive_path.string()
        );
    }
}

void MultiFileArchiveWriter::write_directory(
    const std::filesystem::path& archive_path,
    const std::filesystem::path& directory,
    std::size_t block_size
) {
    if (!std::filesystem::exists(directory) ||
        !std::filesystem::is_directory(directory)) {
        throw std::invalid_argument(
            "Input path is not a directory: " + directory.string()
        );
    }
    if (std::filesystem::is_symlink(
            std::filesystem::symlink_status(directory))) {
        throw std::invalid_argument("Input directory cannot be a symbolic link.");
    }

    const auto root = std::filesystem::canonical(directory);
    const auto output = std::filesystem::weakly_canonical(archive_path);
    if (is_within_directory(root, output)) {
        throw std::invalid_argument(
            "Archive output must be outside the input directory."
        );
    }

    const auto entries = FileEnumerator::enumerate(directory);
    std::vector<ArchiveInput> files;
    files.reserve(entries.size());
    for (const auto& entry : entries) {
        const auto source = directory / entry.relative_path;
        const auto status = std::filesystem::symlink_status(source);
        if (std::filesystem::is_symlink(status) ||
            !std::filesystem::is_regular_file(status)) {
            throw std::invalid_argument(
                "Input tree contains a non-regular file: " + source.string()
            );
        }
        files.push_back({
            entry.relative_path.generic_string(),
            FileReader::read(source)
        });
    }
    write(archive_path, std::move(files), block_size);
}

ArchivedTree MultiFileArchiveReader::read(
    const std::filesystem::path& archive_path
) {
    const auto bytes = read_archive(archive_path);
    if (bytes.size() < kHeaderSize + kIntegritySize) {
        malformed("Archive is truncated or missing its integrity footer.");
    }

    const std::size_t size = bytes.size();
    const std::size_t data_region_size =
        size - static_cast<std::size_t>(kIntegritySize);
    const std::size_t footer_offset = size - static_cast<std::size_t>(kFooterSize);
    if (read_u64(bytes, footer_offset) != 0 ||
        read_u32(bytes, footer_offset + 8) != UATC_MAGIC) {
        malformed("Archive integrity footer is invalid.");
    }

    const auto archive_digest = core::sha256(bytes.data(), data_region_size);
    const std::size_t digest_offset =
        data_region_size + core::kSha256Size;
    if (!std::equal(
            archive_digest.begin(),
            archive_digest.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(digest_offset))) {
        throw core::UatcError(
            core::ErrorCode::ArchiveChecksum,
            "Archive SHA-256 mismatch."
        );
    }

    if (read_u32(bytes, 0) != UATC_MAGIC) {
        malformed("Invalid UATC archive magic.");
    }
    if (read_u16(bytes, 4) != UATC_VERSION) {
        throw core::UatcError(
            core::ErrorCode::UnsupportedVersion,
            "Unsupported UATC archive version."
        );
    }
    if (read_u16(bytes, 6) != kHeaderSize) {
        malformed("Invalid UATC header size.");
    }

    const std::uint32_t file_count = read_u32(bytes, 8);
    const std::uint32_t block_count = read_u32(bytes, 12);
    const std::uint64_t file_table_offset = read_u64(bytes, 16);
    const std::uint64_t block_index_offset = read_u64(bytes, 24);
    const std::uint64_t payload_offset = read_u64(bytes, 32);
    if (file_table_offset < kHeaderSize ||
        file_table_offset > data_region_size ||
        block_index_offset < file_table_offset ||
        block_index_offset > data_region_size ||
        payload_offset < block_index_offset ||
        payload_offset > data_region_size) {
        malformed("Invalid archive section offsets.");
    }

    const std::uint64_t file_table_end = checked_add(
        file_table_offset,
        checked_multiply(file_count, kFileRecordSize)
    );
    const std::uint64_t block_index_end = checked_add(
        block_index_offset,
        checked_multiply(block_count, kBlockRecordSize)
    );
    if (file_table_end > block_index_offset ||
        block_index_end > payload_offset) {
        malformed("Archive metadata sections overlap or exceed their bounds.");
    }

    std::vector<FileLayout> file_layouts(file_count);
    std::uint64_t expected_first_block = 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> path_ranges;
    std::vector<std::string> paths;
    paths.reserve(file_count);

    for (std::uint32_t file_id = 0; file_id < file_count; ++file_id) {
        const std::size_t record = static_cast<std::size_t>(
            file_table_offset + file_id * kFileRecordSize
        );
        const std::uint32_t recorded_id = read_u32(bytes, record);
        const std::uint64_t path_offset = read_u64(bytes, record + 4);
        const std::uint32_t path_length = read_u32(bytes, record + 12);
        const std::uint64_t original_size = read_u64(bytes, record + 16);
        const std::uint32_t first_block = read_u32(bytes, record + 24);
        const std::uint32_t per_file_block_count = read_u32(bytes, record + 28);
        if (recorded_id != file_id || first_block != expected_first_block) {
            malformed("Invalid file ID or first-block index.");
        }
        expected_first_block = checked_add(
            expected_first_block,
            per_file_block_count
        );
        if (expected_first_block > block_count) {
            malformed("File block counts exceed the archive block count.");
        }
        const std::uint64_t path_end = checked_add(path_offset, path_length);
        if (path_length == 0 || path_offset < file_table_end ||
            path_end > block_index_offset) {
            malformed("File path is outside the path table.");
        }
        if (original_size > file_layouts[file_id].data.max_size()) {
            malformed("File exceeds supported size limits.");
        }

        const std::string stored_path(
            reinterpret_cast<const char*>(bytes.data() + path_offset),
            path_length
        );
        if (stored_path.find('\0') != std::string::npos) {
            malformed("File path contains an embedded NUL byte.");
        }
        const std::string safe_path = safe_archive_path(stored_path);
        if (safe_path != stored_path) {
            throw core::UatcError(
                core::ErrorCode::PathTraversal,
                "The archive contains a non-normalized file path: " +
                    stored_path
            );
        }
        paths.push_back(safe_path);
        path_ranges.emplace_back(path_offset, path_end);

        auto& file = file_layouts[file_id];
        file.path = safe_path;
        file.original_size = original_size;
        file.first_block = first_block;
        file.block_count = per_file_block_count;
        file.data.reserve(static_cast<std::size_t>(original_size));
    }

    if (expected_first_block != block_count) {
        malformed("File block counts do not match the archive block count.");
    }
    std::sort(path_ranges.begin(), path_ranges.end());
    for (std::size_t i = 1; i < path_ranges.size(); ++i) {
        if (path_ranges[i].first < path_ranges[i - 1].second) {
            malformed("File paths overlap in the archive.");
        }
    }
    validate_path_set(paths);

    std::vector<BlockLayout> blocks;
    blocks.reserve(block_count);
    std::uint64_t expected_payload_offset = payload_offset;
    std::uint32_t current_file = 0;
    for (std::uint32_t index = 0; index < block_count; ++index) {
        while (current_file < file_count &&
               index >= file_layouts[current_file].first_block +
                            file_layouts[current_file].block_count) {
            ++current_file;
        }
        if (current_file >= file_count) {
            malformed("Block index refers to no file record.");
        }

        const std::size_t record = static_cast<std::size_t>(
            block_index_offset + index * kBlockRecordSize
        );
        BlockLayout block;
        block.file_id = read_u32(bytes, record);
        block.block_id = read_u32(bytes, record + 4);
        block.payload_offset = read_u64(bytes, record + 8);
        block.original_length = read_u64(bytes, record + 16);
        block.stored_length = read_u64(bytes, record + 24);
        block.transform_id = static_cast<TransformId>(bytes[record + 32]);
        block.codec_id = static_cast<CodecId>(bytes[record + 33]);
        block.transform_parameter = read_u16(bytes, record + 34);
        block.checksum = read_u32(bytes, record + 36);

        const auto& file = file_layouts[current_file];
        const std::uint32_t expected_local_id =
            index - file.first_block;
        if (block.file_id != current_file ||
            block.block_id != expected_local_id ||
            block.payload_offset != expected_payload_offset ||
            block.original_length == 0 ||
            block.transform_parameter != 0) {
            malformed("Invalid block index entry.");
        }

        bool known_transform = false;
        for (const auto candidate : core::candidate_transform_ids()) {
            if (block.transform_id == candidate) {
                known_transform = true;
                break;
            }
        }
        if (!known_transform ||
            (block.codec_id != CodecId::None &&
             block.codec_id != CodecId::Baseline)) {
            malformed("Archive uses an unsupported transform or codec.");
        }
        if (block.transform_id == TransformId::None &&
            block.codec_id == CodecId::None &&
            block.stored_length != block.original_length) {
            malformed("Raw block length does not match its original length.");
        }
        if (block.stored_length >
            std::numeric_limits<std::size_t>::max()) {
            malformed("Stored block exceeds supported size limits.");
        }

        expected_payload_offset = checked_add(
            expected_payload_offset,
            block.stored_length
        );
        if (expected_payload_offset > data_region_size) {
            malformed("Block payload extends beyond the archive.");
        }
        blocks.push_back(block);
    }
    if (expected_payload_offset != data_region_size) {
        malformed("Payload data does not end at the integrity region.");
    }

    for (const auto& block : blocks) {
        const auto first = bytes.begin() +
            static_cast<std::ptrdiff_t>(block.payload_offset);
        const auto last = first + static_cast<std::ptrdiff_t>(block.stored_length);
        std::vector<std::uint8_t> stored(first, last);
        std::vector<std::uint8_t> decoded;
        if (block.transform_id == TransformId::None &&
            block.codec_id == CodecId::None) {
            decoded = std::move(stored);
        } else {
            decoded = Codec::decode(stored, block.codec_id);
            decoded = Transform::decode(decoded, block.transform_id);
        }
        if (decoded.size() != block.original_length) {
            malformed("Decoded block length does not match its metadata.");
        }
        if (core::crc32(decoded.data(), decoded.size()) != block.checksum) {
            throw core::UatcError(
                core::ErrorCode::BlockChecksum,
                "Block checksum mismatch at file " +
                    std::to_string(block.file_id) + ", block " +
                    std::to_string(block.block_id) + "."
            );
        }

        auto& file = file_layouts[block.file_id];
        if (decoded.size() > file.data.max_size() - file.data.size()) {
            malformed("Reconstructed file exceeds supported size limits.");
        }
        file.data.insert(file.data.end(), decoded.begin(), decoded.end());
    }

    auto file_hash = core::sha256_begin();
    ArchivedTree tree;
    tree.files.reserve(file_layouts.size());
    for (auto& file : file_layouts) {
        if (file.data.size() != file.original_size) {
            malformed("Reconstructed file size does not match its metadata.");
        }
        core::sha256_feed(file_hash, file.data.data(), file.data.size());
        tree.files.push_back({
            file.path,
            file.original_size,
            std::move(file.data)
        });
    }

    const auto file_digest = core::sha256_end(file_hash);
    if (!std::equal(
            file_digest.begin(),
            file_digest.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(data_region_size))) {
        throw core::UatcError(
            core::ErrorCode::ArchiveChecksum,
            "File SHA-256 mismatch."
        );
    }

    return tree;
}

void MultiFileArchiveReader::extract(
    const std::filesystem::path& archive_path,
    const std::filesystem::path& output_directory
) {
    const ArchivedTree tree = read(archive_path);
    const auto root = std::filesystem::absolute(output_directory).lexically_normal();
    preflight_destination(root, tree.files);

    std::vector<std::filesystem::path> created_files;
    std::vector<std::filesystem::path> created_directories;
    try {
        std::vector<std::filesystem::path> missing_roots;
        auto current = root;
        while (!has_existing_entry(current)) {
            missing_roots.push_back(current);
            const auto parent = current.parent_path();
            if (parent == current || current.empty()) {
                throw std::runtime_error(
                    "Cannot create extraction destination: " + root.string()
                );
            }
            current = parent;
        }
        for (auto it = missing_roots.rbegin(); it != missing_roots.rend(); ++it) {
            if (!std::filesystem::create_directory(*it)) {
                throw std::runtime_error(
                    "Cannot create extraction directory: " + it->string()
                );
            }
            created_directories.push_back(*it);
        }

        for (const auto& file : tree.files) {
            const auto components = path_components(file.relative_path);
            auto parent = root;
            for (std::size_t i = 0; i + 1 < components.size(); ++i) {
                parent /= components[i];
                if (!has_existing_entry(parent)) {
                    if (!std::filesystem::create_directory(parent)) {
                        throw std::runtime_error(
                            "Cannot create extraction directory: " +
                            parent.string()
                        );
                    }
                    created_directories.push_back(parent);
                }
            }

            const auto target = root / std::filesystem::path(file.relative_path);
            std::ofstream output(
                target,
                std::ios::binary | std::ios::out | std::ios::trunc
            );
            if (!output) {
                throw std::runtime_error(
                    "Cannot create extracted file: " + target.string()
                );
            }
            created_files.push_back(target);
            if (!file.data.empty()) {
                output.write(
                    reinterpret_cast<const char*>(file.data.data()),
                    static_cast<std::streamsize>(file.data.size())
                );
            }
            output.close();
            if (!output) {
                throw std::runtime_error(
                    "Failed while writing extracted file: " + target.string()
                );
            }
        }
    } catch (...) {
        remove_created_paths(created_files, created_directories);
        throw;
    }
}

}  // namespace uatc
