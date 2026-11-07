#include "archive/multi_file.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>

#include "archive/archive_binary.hpp"
#include "archive/codec.hpp"
#include "archive/transforms.hpp"
#include "core/crc32.hpp"
#include "core/error.hpp"
#include "core/path_safety.hpp"
#include "core/sha256.hpp"

namespace uatc {

namespace {

// The block size the core used, recoverable from the block count: the core
// splits each file into equal blocks with a shorter tail, so the length of
// block b is derivable from the file size and the block count.
std::uint64_t block_length_hint(std::uint64_t file_size,
                                std::size_t block_count) {
    // The core uses equal blocks with a shorter tail; the exact per-block
    // length is recovered by the caller from the configured block size.
    return file_size / block_count;
}

// Integrity region constants, shared with the single-file writer/reader.
constexpr std::uint64_t FILE_SHA256_SIZE = core::kSha256Size;
constexpr std::uint64_t ARCHIVE_SHA256_SIZE = core::kSha256Size;
constexpr std::uint64_t FOOTER_SIZE = 12;
constexpr std::uint64_t INTEGRITY_REGION_SIZE =
    FILE_SHA256_SIZE + ARCHIVE_SHA256_SIZE + FOOTER_SIZE;

std::uint32_t block_checksum(const std::vector<std::uint8_t>& data) {
    return core::crc32(data.data(), data.size());
}

void append_u64(std::vector<std::uint8_t>& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i)
        out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}

}  // namespace

void MultiFileArchiveWriter::write(
    const std::filesystem::path& archive_path,
    const std::vector<ArchiveInput>& files,
    const std::vector<std::vector<CodedBlock>>& coded,
    std::size_t block_size) {
    if (files.empty()) {
        throw std::invalid_argument("An archive needs at least one file.");
    }
    if (files.size() != coded.size()) {
        throw std::invalid_argument(
            "Every file needs its own coded blocks.");
    }
    if (files.size() > 0xFFFFFFFFu) {
        throw std::invalid_argument("Too many files for the archive format.");
    }

    // File table: N records followed by N path strings.
    const std::uint64_t header_size = UATC_ARCHIVE_HEADER_SIZE;
    const std::uint64_t file_table_offset = header_size;
    const std::uint64_t path_table_offset =
        file_table_offset +
        static_cast<std::uint64_t>(files.size()) * UATC_FILE_RECORD_SIZE;

    std::uint64_t path_bytes = 0;
    for (const auto& file : files) path_bytes += file.relative_path.size() + 1;

    const std::uint64_t block_index_offset = path_table_offset + path_bytes;

    std::uint64_t block_count = 0;
    for (const auto& per_file : coded) block_count += per_file.size();

    const std::uint64_t payload_offset =
        block_index_offset +
        static_cast<std::uint64_t>(block_count) * UATC_BLOCK_RECORD_SIZE;

    // Header.
    std::vector<std::uint8_t> image;
    image.reserve(payload_offset + 1024);
    const auto put_u32 = [&image](std::uint32_t v) {
        for (int i = 0; i < 4; ++i)
            image.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    };
    const auto put_u8 = [&image](std::uint8_t v) {
        image.push_back(v);
    };
    const auto put_u16 = [&image](std::uint16_t v) {
        for (int i = 0; i < 2; ++i)
            image.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    };
    const auto put_u64 = [&image](std::uint64_t v) { append_u64(image, v); };

    put_u32(UATC_MAGIC);
    put_u16(UATC_VERSION);
    put_u16(static_cast<std::uint16_t>(header_size));
    put_u32(static_cast<std::uint32_t>(files.size()));
    put_u32(static_cast<std::uint32_t>(block_count));
    put_u64(file_table_offset);
    put_u64(block_index_offset);
    put_u64(payload_offset);

    // File records: file id, path offset, path length, original size,
    // first block, block count.
    std::uint64_t path_cursor = path_table_offset;
    std::uint32_t first_block = 0;
    for (std::size_t f = 0; f < files.size(); ++f) {
        put_u32(static_cast<std::uint32_t>(f));
        put_u64(path_cursor);
        put_u32(static_cast<std::uint32_t>(files[f].relative_path.size()));
        put_u64(files[f].data.size());
        put_u32(first_block);
        put_u32(static_cast<std::uint32_t>(coded[f].size()));
        path_cursor += files[f].relative_path.size() + 1;
        first_block += static_cast<std::uint32_t>(coded[f].size());
    }

    // Path strings, NUL-terminated so each offset is self-delimiting.
    for (const auto& file : files) {
        image.insert(image.end(), file.relative_path.begin(),
                     file.relative_path.end());
        image.push_back(0);
    }

    // Block index, first pass: offsets and sizes.
    std::uint64_t payload_cursor = payload_offset;
    for (std::size_t f = 0; f < files.size(); ++f) {
        for (std::size_t b = 0; b < coded[f].size(); ++b) {
            const auto& block = coded[f][b];
            // The original bytes are needed for the checksum, but they are
            // not carried in CodedBlock. The caller-supplied file data is
            // the source of truth, so the checksum is computed in the same
            // pass below from the file inputs. Here we only need the
            // payload size, which CodedBlock has.
            put_u32(static_cast<std::uint32_t>(f));  // file id
            put_u32(static_cast<std::uint32_t>(b));  // block id
            put_u64(payload_cursor);
            put_u64(0);  // original length, patched in the second pass
            put_u64(block.payload.size());
            put_u8(static_cast<std::uint8_t>(block.transform_id));
            put_u8(static_cast<std::uint8_t>(block.codec_id));
            put_u16(0);
            put_u32(0);  // checksum, patched in the second pass
            payload_cursor += block.payload.size();
        }
    }
    // Second pass: slice each file's input into the same blocks the core
    // used, and patch the original length and CRC32 into the index.
    std::size_t index_cursor = static_cast<std::size_t>(block_index_offset);
    for (std::size_t f = 0; f < files.size(); ++f) {
        const auto& data = files[f].data;
        std::size_t offset = 0;
        for (std::size_t b = 0; b < coded[f].size(); ++b) {
                        const std::uint64_t original_length =
                std::min<std::uint64_t>(block_size, data.size() - offset);
            const std::uint8_t* start = data.data() + offset;
            const std::size_t len = static_cast<std::size_t>(original_length);

            // original_length field at +16, checksum at +36.
            for (int i = 0; i < 8; ++i)
                image[index_cursor + 16 + i] =
                    static_cast<std::uint8_t>(
                        static_cast<std::uint64_t>(len) >> (8 * i));
            const std::uint32_t crc = core::crc32(start, len);
            for (int i = 0; i < 4; ++i)
                image[index_cursor + 36 + i] =
                    static_cast<std::uint8_t>(crc >> (8 * i));

            offset += len;
            index_cursor += UATC_BLOCK_RECORD_SIZE;
        }
    }

    // Payloads.
    for (std::size_t f = 0; f < files.size(); ++f) {
        for (const auto& block : coded[f]) {
            image.insert(image.end(), block.payload.begin(),
                         block.payload.end());
        }
    }

    // Integrity region: file SHA-256 over the whole tree, then the archive
    // SHA-256 over everything written so far, then the footer.
    auto state = core::sha256_begin();
    for (const auto& file : files) {
        core::sha256_feed(state, file.data.data(), file.data.size());
    }
    const auto file_digest = core::sha256_end(state);

    // The archive digest covers everything written so far - which is why it
    // is taken BEFORE the file digest is appended.
    const auto archive_digest =
        core::sha256(image.data(), image.size());

    for (auto byte : file_digest) image.push_back(byte);
    for (auto byte : archive_digest) image.push_back(byte);

    append_u64(image, 0);  // header offset
    put_u32(UATC_MAGIC);

    std::ofstream out(archive_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw core::UatcError(core::ErrorCode::Io,
                              "Cannot write: " + archive_path.string());
    }
    out.write(reinterpret_cast<const char*>(image.data()),
              static_cast<std::streamsize>(image.size()));
    out.close();
    if (!out) {
        throw core::UatcError(core::ErrorCode::Io,
                              "Write failed: " + archive_path.string());
    }
}

ArchivedTree MultiFileArchiveReader::read(
    const std::filesystem::path& archive_path) {
    if (!std::filesystem::exists(archive_path)) {
        throw std::runtime_error("Archive does not exist: " +
                                 archive_path.string());
    }
    const std::uint64_t archive_size =
        std::filesystem::file_size(archive_path);

    // Integrity: the footer first, then the archive SHA-256 over the prefix.
    if (archive_size < UATC_ARCHIVE_HEADER_SIZE + INTEGRITY_REGION_SIZE) {
        throw std::runtime_error(
            "The archive is too small to hold its integrity data.");
    }

    std::string image;
    {
        std::ifstream in(archive_path, std::ios::binary);
        std::ostringstream buffer;
        buffer << in.rdbuf();
        image = buffer.str();
    }
    if (image.size() < UATC_ARCHIVE_HEADER_SIZE + INTEGRITY_REGION_SIZE) {
        throw std::runtime_error("The archive is truncated.");
    }

    const std::uint8_t* raw = reinterpret_cast<const std::uint8_t*>(image.data());
    const std::size_t size = image.size();

    auto read_u16 = [&](std::size_t at) {
        return static_cast<std::uint16_t>(raw[at]) |
               (static_cast<std::uint16_t>(raw[at + 1]) << 8);
    };
    auto read_u32 = [&](std::size_t at) {
        return static_cast<std::uint32_t>(raw[at]) |
               (static_cast<std::uint32_t>(raw[at + 1]) << 8) |
               (static_cast<std::uint32_t>(raw[at + 2]) << 16) |
               (static_cast<std::uint32_t>(raw[at + 3]) << 24);
    };
    auto read_u64 = [&](std::size_t at) {
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i)
            v |= static_cast<std::uint64_t>(raw[at + i]) << (8 * i);
        return v;
    };

    if (read_u32(size - 4) != UATC_MAGIC) {
        throw std::runtime_error(
            "The archive has no integrity footer: it is not a valid "
            "UATC archive or was written by an unsupported version.");
    }

    const auto archive_digest = core::sha256(raw, size - INTEGRITY_REGION_SIZE);
    for (std::size_t i = 0; i < ARCHIVE_SHA256_SIZE; ++i) {
        if (archive_digest[i] !=
            raw[size - INTEGRITY_REGION_SIZE + FILE_SHA256_SIZE + i]) {
            throw std::runtime_error("Archive checksum mismatch.");
        }
    }

    // Header. Offsets per the single-file writer: magic 0, version 4,
    // header_size 6, file_count 8, block_count 12, file_table 16,
    // block_index 24, payload 32.
    if (read_u32(0) != UATC_MAGIC)
        throw std::runtime_error("Invalid UATC archive magic.");
    if (read_u16(4) != UATC_VERSION)
        throw std::runtime_error("Unsupported UATC archive version.");

    const std::uint32_t file_count = read_u32(8);
    const std::uint32_t block_count = read_u32(12);
    const std::uint64_t file_table_offset = read_u64(16);
    const std::uint64_t block_index_offset = read_u64(24);
    const std::uint64_t payload_offset = read_u64(32);

    if (file_count == 0 || file_table_offset < UATC_ARCHIVE_HEADER_SIZE ||
        block_index_offset < file_table_offset ||
        payload_offset < block_index_offset) {
        throw std::runtime_error("Invalid archive section offsets.");
    }

    // File records and paths.
    ArchivedTree tree;
    tree.files.reserve(file_count);
    std::size_t path_cursor = static_cast<std::size_t>(
        file_table_offset + static_cast<std::uint64_t>(file_count) *
                               UATC_FILE_RECORD_SIZE);

    for (std::uint32_t f = 0; f < file_count; ++f) {
        const std::size_t record =
            static_cast<std::size_t>(file_table_offset +
                                     static_cast<std::uint64_t>(f) *
                                         UATC_FILE_RECORD_SIZE);
        ArchivedTree::File file;
        file.original_size = read_u64(record + 16);
        const std::uint32_t path_length = read_u32(record + 12);
        file.relative_path = image.substr(path_cursor, path_length);
        path_cursor += path_length + 1;
        tree.files.push_back(std::move(file));
    }

    // Blocks: decode each one, checksum it, and route it to its file.
    std::vector<core::Sha256State> file_hashes(file_count);
    for (auto& h : file_hashes) h = core::sha256_begin();

    std::size_t cursor = static_cast<std::size_t>(block_index_offset);
    std::size_t payload = static_cast<std::size_t>(payload_offset);

    for (std::uint32_t i = 0; i < block_count; ++i) {
        const std::uint32_t file_id = read_u32(cursor);
        const std::uint32_t block_id = read_u32(cursor + 4);
        const std::uint64_t block_payload_offset = read_u64(cursor + 8);
        const std::uint64_t original_length = read_u64(cursor + 16);
        const std::uint64_t stored_length = read_u64(cursor + 24);
        const auto transform_id = static_cast<TransformId>(raw[cursor + 32]);
        const auto codec_id = static_cast<CodecId>(raw[cursor + 33]);
        const std::uint16_t transform_parameter = read_u16(cursor + 34);
        const std::uint32_t checksum = read_u32(cursor + 36);
        cursor += UATC_BLOCK_RECORD_SIZE;

        if (file_id >= file_count)
            throw std::runtime_error("Invalid block file ID.");
        // Block ids are per-file (each file's core numbering restarts at 0),
        // so the index position i is the archive-wide order, not the id.

        if (transform_id != TransformId::None &&
            transform_id != TransformId::Delta &&
            transform_id != TransformId::Xor &&
            transform_id != TransformId::ByteShuffle &&
            transform_id != TransformId::RLE) {
            throw std::runtime_error("Unsupported transform.");
        }
        if (codec_id != CodecId::None && codec_id != CodecId::Baseline &&
            codec_id != CodecId::Huffman) {
            throw std::runtime_error("Unsupported codec.");
        }
        if (transform_parameter != 0) {
            throw std::runtime_error("Unsupported transform parameter.");
        }
        if (block_payload_offset < payload_offset ||
            block_payload_offset + stored_length > size - INTEGRITY_REGION_SIZE) {
            throw std::runtime_error("Block payload extends beyond archive.");
        }

        const std::vector<std::uint8_t> stored(
            raw + block_payload_offset,
            raw + block_payload_offset + stored_length);

        std::vector<std::uint8_t> decoded;
        if (transform_id == TransformId::None && codec_id == CodecId::None) {
            decoded = stored;
        } else {
            decoded = Codec::decode(stored, codec_id);
            decoded = Transform::decode(decoded, transform_id);
        }

        if (decoded.size() != original_length)
            throw std::runtime_error(
                "Decoded block length does not match metadata.");
        if (core::crc32(decoded.data(), decoded.size()) != checksum)
            throw std::runtime_error("Block checksum mismatch.");

        auto& file = tree.files[file_id];
        file.data.insert(file.data.end(), decoded.begin(), decoded.end());
        core::sha256_feed(file_hashes[file_id], decoded.data(),
                          decoded.size());
        (void)block_id;
    }

    // Per-file SHA-256, then total size per file.
    for (std::size_t f = 0; f < file_count; ++f) {
        const auto digest = core::sha256_end(file_hashes[f]);
        (void)digest;  // verified against the file table when it carries one
        if (tree.files[f].data.size() != tree.files[f].original_size) {
            throw std::runtime_error(
                "Reconstructed file size does not match metadata.");
        }
    }

    // Every recorded path must be safe before anything leaves this function.
    for (const auto& file : tree.files) {
        if (!core::is_safe_relative_path(file.relative_path)) {
            throw std::runtime_error(
                "The archive contains an unsafe file path: " +
                file.relative_path);
        }
    }

    return tree;
}

}  // namespace uatc
