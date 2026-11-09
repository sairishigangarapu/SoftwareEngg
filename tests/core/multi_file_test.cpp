#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "archive/archive_reader.hpp"
#include "archive/archive_writer.hpp"
#include "archive/archive_inspector.hpp"
#include "archive/archive_verifier.hpp"
#include "archive/block.hpp"
#include "archive/multi_file.hpp"
#include "core/error.hpp"
#include "core/sha256.hpp"
#include "filesystem/file_reader.hpp"

namespace fs = std::filesystem;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class TestDirectory {
public:
    TestDirectory() {
        const auto stamp = std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count();
        path_ = fs::temp_directory_path() /
                ("uatc-multi-file-" + std::to_string(stamp));
        fs::create_directories(path_);
    }

    ~TestDirectory() {
        std::error_code ignored;
        fs::remove_all(path_, ignored);
    }

    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

std::vector<std::uint8_t> read_bytes(const fs::path& path) {
    return uatc::FileReader::read(path);
}

void write_bytes(const fs::path& path,
                 const std::vector<std::uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "could not create test file");
    if (!bytes.empty()) {
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size())
        );
    }
    output.close();
    require(static_cast<bool>(output), "could not write test file");
}

void write_text(const fs::path& path, const std::string& value) {
    write_bytes(
        path,
        std::vector<std::uint8_t>(value.begin(), value.end())
    );
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& data,
                       std::size_t offset) {
    std::uint32_t value = 0;
    for (unsigned int i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(data[offset + i]) << (8 * i);
    }
    return value;
}

std::uint64_t read_u64(const std::vector<std::uint8_t>& data,
                       std::size_t offset) {
    std::uint64_t value = 0;
    for (unsigned int i = 0; i < 8; ++i) {
        value |= static_cast<std::uint64_t>(data[offset + i]) << (8 * i);
    }
    return value;
}

void write_u32(std::vector<std::uint8_t>& data,
               std::size_t offset,
               std::uint32_t value) {
    for (unsigned int i = 0; i < 4; ++i) {
        data[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
    }
}

void update_archive_digest(std::vector<std::uint8_t>& archive) {
    constexpr std::size_t integrity_size = 76;
    constexpr std::size_t sha_size = uatc::core::kSha256Size;
    const std::size_t prefix_size = archive.size() - integrity_size;
    const auto digest =
        uatc::core::sha256(archive.data(), prefix_size);
    std::copy(
        digest.begin(),
        digest.end(),
        archive.begin() + static_cast<std::ptrdiff_t>(
                              prefix_size + sha_size
                          )
    );
}

template <typename Callback>
void require_throws(Callback callback, const std::string& message) {
    bool threw = false;
    try {
        callback();
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, message);
}

void require_error_code(
    const fs::path& archive,
    uatc::core::ErrorCode expected,
    const std::string& message
) {
    try {
        (void)uatc::MultiFileArchiveReader::read(archive);
    } catch (const uatc::core::UatcError& error) {
        require(error.code() == expected, message);
        return;
    }
    throw std::runtime_error(message);
}

void verify_tree(
    const fs::path& source,
    const fs::path& restored,
    const uatc::ArchivedTree& tree
) {
    for (const auto& file : tree.files) {
        const auto source_file = source / fs::path(file.relative_path);
        const auto restored_file = restored / fs::path(file.relative_path);
        const auto expected = read_bytes(source_file);
        const auto actual = read_bytes(restored_file);
        require(expected == actual, "restored bytes differ");
        require(
            uatc::core::sha256_hex(
                uatc::core::sha256(actual.data(), actual.size())
            ) ==
                uatc::core::sha256_hex(
                    uatc::core::sha256(expected.data(), expected.size())
                ),
            "restored SHA-256 differs"
        );
    }
}

void test_directory_round_trip_and_determinism(const fs::path& root) {
    const auto source = root / "input";
    write_text(source / "top-level.txt", "a file at the root\n");
    write_text(source / "nested" / "name with spaces.txt",
               "nested text repeated repeated repeated\n");
    write_bytes(source / "nested" / "empty.bin", {});
    std::vector<std::uint8_t> binary(4097);
    for (std::size_t i = 0; i < binary.size(); ++i) {
        binary[i] = static_cast<std::uint8_t>((i * 73 + 19) & 0xff);
    }
    write_bytes(source / "nested" / "binary.dat", binary);

    const auto archive = root / "tree.uatc";
    const auto second_archive = root / "tree-again.uatc";
    uatc::MultiFileArchiveWriter::write_directory(archive, source, 1024);
    uatc::MultiFileArchiveWriter::write_directory(
        second_archive,
        source,
        1024
    );
    require_throws(
        [&] {
            uatc::MultiFileArchiveWriter::write_directory(
                source / "inside.uatc",
                source,
                1024
            );
        },
        "writer allowed its archive output inside the input directory"
    );
    require(
        read_bytes(archive) == read_bytes(second_archive),
        "directory archives must be deterministic"
    );

    const auto tree = uatc::MultiFileArchiveReader::read(archive);
    require(tree.files.size() == 4, "wrong file count");
    require(tree.files[0].relative_path == "nested/binary.dat",
            "file table is not sorted");
    require(tree.files[1].relative_path == "nested/empty.bin",
            "empty file was not recorded");

    const auto info = uatc::ArchiveInspector::inspect(archive);
    require(info.fileCount == tree.files.size(),
            "inspector file count mismatch");
    require(info.files.size() == tree.files.size(),
            "inspector omitted per-file records");
    for (std::size_t i = 0; i < info.files.size(); ++i) {
        require(info.files[i].relativePath == tree.files[i].relative_path,
                "inspector file path mismatch");
        require(info.files[i].originalBytes == tree.files[i].original_size,
                "inspector per-file size mismatch");
    }
    const auto verification = uatc::ArchiveVerifier::verify(archive);
    require(verification.success, "multi-file verification failed");
    require(verification.filesChecked == tree.files.size(),
            "verifier file count mismatch");

    const auto restored = root / "restored";
    uatc::MultiFileArchiveReader::extract(archive, restored);
    verify_tree(source, restored, tree);
}

void test_empty_directory(const fs::path& root) {
    const auto source = root / "empty-source";
    fs::create_directories(source);
    const auto archive = root / "empty.uatc";
    const auto restored = root / "empty-restored";

    uatc::MultiFileArchiveWriter::write_directory(archive, source, 1024);
    const auto tree = uatc::MultiFileArchiveReader::read(archive);
    require(tree.files.empty(), "empty directory should have no file records");
    const auto info = uatc::ArchiveInspector::inspect(archive);
    require(info.fileCount == 0 && info.originalBytes == 0,
            "empty archive metadata is incorrect");
    require(uatc::ArchiveVerifier::verify(archive).success,
            "empty directory archive failed verification");
    uatc::MultiFileArchiveReader::extract(archive, restored);
    require(fs::is_directory(restored), "empty extraction directory missing");
    require(fs::is_empty(restored), "empty extraction is not empty");
}

void test_input_order_and_path_validation(const fs::path& root) {
    std::vector<uatc::ArchiveInput> files{
        {"z/last.bin", {9, 8, 7}},
        {"a/first.bin", {1, 2, 3}}
    };
    auto reversed = files;
    std::reverse(reversed.begin(), reversed.end());

    const auto first = root / "order-a.uatc";
    const auto second = root / "order-b.uatc";
    uatc::MultiFileArchiveWriter::write(first, files, 2);
    uatc::MultiFileArchiveWriter::write(second, reversed, 2);
    require(read_bytes(first) == read_bytes(second),
            "input order must not affect archive bytes");

    for (const auto& unsafe : {
             "../escape.txt",
             "nested/../../escape.txt",
             "C:/escape.txt",
             "\\\\server\\share\\escape.txt",
             "nested\\..\\escape.txt"}) {
        require_throws(
            [&] {
                uatc::MultiFileArchiveWriter::write(
                    root / "unsafe.uatc",
                    {{unsafe, {1}}},
                    1024
                );
            },
            "writer accepted an unsafe path"
        );
    }

    require_throws(
        [&] {
            uatc::MultiFileArchiveWriter::write(
                root / "duplicate.uatc",
                {{"same.bin", {1}}, {"same.bin", {2}}},
                1024
            );
        },
        "writer accepted duplicate paths"
    );
    require_throws(
        [&] {
            uatc::MultiFileArchiveWriter::write(
                root / "file-parent-conflict.uatc",
                {{"parent", {1}}, {"parent/child", {2}}},
                1024
            );
        },
        "writer accepted a file/parent path conflict"
    );
    require_throws(
        [&] {
            uatc::MultiFileArchiveWriter::write(
                root / "case-conflict.uatc",
                {{"Folder/file.txt", {1}}, {"folder/FILE.txt", {2}}},
                1024
            );
        },
        "writer accepted paths that collide on case-insensitive filesystems"
    );
}

void test_single_file_compatibility(const fs::path& root) {
    const auto source = root / "legacy single.bin";
    std::vector<std::uint8_t> expected(257);
    for (std::size_t i = 0; i < expected.size(); ++i) {
        expected[i] = static_cast<std::uint8_t>(i);
    }
    write_bytes(source, expected);

    const auto blocks = uatc::BlockSplitter::split(expected, 64);
    const auto archive = root / "single.uatc";
    uatc::ArchiveWriter::write(archive, source, blocks);

    const auto tree = uatc::MultiFileArchiveReader::read(archive);
    require(tree.files.size() == 1, "single-file archive was not readable");
    require(tree.files[0].relative_path == source.filename().generic_string(),
            "single-file archive path changed");
    require(tree.files[0].data == expected,
            "single-file archive bytes changed");
    require(uatc::ArchiveReader::read(archive) == expected,
            "existing single-file reader regressed");
    const auto restored = root / "single-restored";
    uatc::MultiFileArchiveReader::extract(archive, restored);
    require(
        read_bytes(restored / source.filename()) == expected,
        "single-file archive extraction regressed"
    );
}

void test_corruption_and_footer_errors(const fs::path& root) {
    std::vector<std::uint8_t> binary(512);
    for (std::size_t i = 0; i < binary.size(); ++i) {
        binary[i] = static_cast<std::uint8_t>((i * 113 + 37) & 0xff);
    }
    const auto valid = root / "valid.uatc";
    uatc::MultiFileArchiveWriter::write(
        valid,
        {{"payload.bin", binary}},
        512
    );
    const auto original = read_bytes(valid);
    const auto payload_offset = static_cast<std::size_t>(read_u64(original, 32));

    auto damaged_payload = original;
    damaged_payload[payload_offset] ^= 0x80;
    update_archive_digest(damaged_payload);
    const auto block_corrupt = root / "block-corrupt.uatc";
    write_bytes(block_corrupt, damaged_payload);
    require_error_code(
        block_corrupt,
        uatc::core::ErrorCode::BlockChecksum,
        "block CRC32 failure was not reported"
    );
    require(!uatc::ArchiveVerifier::verify(block_corrupt).success,
            "verifier accepted a corrupted multi-file archive");

    auto damaged_file_digest = original;
    const std::size_t integrity_offset = original.size() - 76;
    damaged_file_digest[integrity_offset] ^= 0x01;
    update_archive_digest(damaged_file_digest);
    const auto file_digest_corrupt = root / "file-digest-corrupt.uatc";
    write_bytes(file_digest_corrupt, damaged_file_digest);
    require_error_code(
        file_digest_corrupt,
        uatc::core::ErrorCode::ArchiveChecksum,
        "file SHA-256 failure was not reported"
    );

    auto damaged_archive_digest = original;
    damaged_archive_digest[original.size() - 44] ^= 0x01;
    const auto archive_digest_corrupt = root / "archive-digest-corrupt.uatc";
    write_bytes(archive_digest_corrupt, damaged_archive_digest);
    require_error_code(
        archive_digest_corrupt,
        uatc::core::ErrorCode::ArchiveChecksum,
        "archive SHA-256 failure was not reported"
    );

    auto invalid_header = original;
    invalid_header[0] ^= 0x01;
    update_archive_digest(invalid_header);
    const auto invalid_archive = root / "invalid-header.uatc";
    write_bytes(invalid_archive, invalid_header);
    require_error_code(
        invalid_archive,
        uatc::core::ErrorCode::MalformedArchive,
        "invalid archive header was not rejected"
    );

    auto damaged_footer = original;
    damaged_footer.back() ^= 0x01;
    const auto footer_corrupt = root / "footer-corrupt.uatc";
    write_bytes(footer_corrupt, damaged_footer);
    require_throws(
        [&] { (void)uatc::MultiFileArchiveReader::read(footer_corrupt); },
        "invalid footer was accepted"
    );

    auto truncated = original;
    truncated.resize(truncated.size() - 5);
    const auto truncated_archive = root / "truncated.uatc";
    write_bytes(truncated_archive, truncated);
    require_throws(
        [&] { (void)uatc::MultiFileArchiveReader::read(truncated_archive); },
        "truncated archive was accepted"
    );

    const auto partial_output = root / "corrupt-output";
    require_throws(
        [&] {
            uatc::MultiFileArchiveReader::extract(
                block_corrupt,
                partial_output
            );
        },
        "corrupt archive was extracted"
    );
    require(!fs::exists(partial_output),
            "corrupt archive left partial output");
}

void test_path_traversal_and_safe_output(const fs::path& root) {
    const auto archive = root / "path.uatc";
    uatc::MultiFileArchiveWriter::write(
        archive,
        {{"safe/a.txt", {1, 2, 3}}},
        1024
    );
    auto malicious = read_bytes(archive);
    const auto path_offset = static_cast<std::size_t>(read_u64(malicious, 44));
    const auto path_length = read_u32(malicious, 52);
    const std::string traversal = "../bad.txt";
    require(path_length == traversal.size(),
            "test traversal path length does not match");
    std::copy(
        traversal.begin(),
        traversal.end(),
        malicious.begin() + static_cast<std::ptrdiff_t>(path_offset)
    );
    update_archive_digest(malicious);
    const auto unsafe_archive = root / "unsafe-path.uatc";
    write_bytes(unsafe_archive, malicious);

    const auto rejected_output = root / "must-not-exist";
    require_throws(
        [&] {
            uatc::MultiFileArchiveReader::extract(
                unsafe_archive,
                rejected_output
            );
        },
        "extractor accepted a traversal path"
    );
    require(!fs::exists(rejected_output),
            "unsafe extraction created partial output");
    require(!fs::exists(root / "bad.txt"),
            "unsafe extraction wrote outside its destination");

    const auto valid_archive = root / "collision.uatc";
    uatc::MultiFileArchiveWriter::write(
        valid_archive,
        {{"nested/keep.txt", {4, 5, 6}}},
        1024
    );
    const auto destination = root / "collision-output";
    write_text(destination / "nested" / "keep.txt", "do not replace");
    require_throws(
        [&] {
            uatc::MultiFileArchiveReader::extract(
                valid_archive,
                destination
            );
        },
        "extractor overwrote an existing file"
    );
    require(
        read_bytes(destination / "nested" / "keep.txt") ==
            std::vector<std::uint8_t>(
                {'d', 'o', ' ', 'n', 'o', 't', ' ', 'r', 'e', 'p', 'l', 'a', 'c', 'e'}
            ),
        "existing output file changed after collision"
    );
}

void test_large_tree_sha_round_trip(const fs::path& root) {
    const auto source = root / "large-tree";
    for (std::size_t file = 0; file < 64; ++file) {
        std::vector<std::uint8_t> bytes(1536 + file * 17);
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            bytes[i] = static_cast<std::uint8_t>(
                (i * 31 + file * 7 + (i / 9)) & 0xff
            );
        }
        write_bytes(
            source / ("level-" + std::to_string(file % 4)) /
                ("file-" + std::to_string(file) + ".dat"),
            bytes
        );
    }

    const auto archive = root / "large-tree.uatc";
    uatc::MultiFileArchiveWriter::write_directory(archive, source, 4096);
    const auto tree = uatc::MultiFileArchiveReader::read(archive);
    require(tree.files.size() == 64, "large tree file count mismatch");

    const auto restored = root / "large-tree-restored";
    uatc::MultiFileArchiveReader::extract(archive, restored);
    verify_tree(source, restored, tree);
}

}  // namespace

int main() {
    try {
        TestDirectory temporary;
        test_directory_round_trip_and_determinism(temporary.path());
        test_empty_directory(temporary.path());
        test_input_order_and_path_validation(temporary.path());
        test_single_file_compatibility(temporary.path());
        test_corruption_and_footer_errors(temporary.path());
        test_path_traversal_and_safe_output(temporary.path());
        test_large_tree_sha_round_trip(temporary.path());
        std::cout << "Multi-file archive tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Multi-file archive test failed: " << error.what() << '\n';
        return 1;
    }
}
