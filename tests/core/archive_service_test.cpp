// Core facade + folder archives through info/verify (FR-01/02/03, FR-09,
// FR-11, FR-12; SAD 4.3.3).
//
// TC-ARC-03  folder round trip through compress_path / extract_path
// TC-INFO-01 info on a folder archive: file count, sizes, ratio, file list
// TC-VER-01  verify on an intact folder archive
// TC-INT-01  a flipped payload byte -> E_BLOCK_CHECKSUM from the verifier
// TC-ERR-03  a non-archive -> E_MALFORMED_ARCHIVE from the verifier
// TC-PAR-01  the folder archive is byte-identical for 1 and 4 workers
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "archive/archive_inspector.hpp"
#include "archive/archive_verifier.hpp"
#include "core/archive_service.hpp"
#include "core/error.hpp"
#include "core/sha256.hpp"

namespace fs = std::filesystem;
using uatc::core::ErrorCode;

namespace {

int failures = 0;

void check(bool ok, const std::string& name, const std::string& detail = "") {
    if (ok) {
        std::cout << "PASS: " << name << "\n";
    } else {
        std::cerr << "FAIL: " << name << (detail.empty() ? "" : "  -- " + detail)
                  << "\n";
        ++failures;
    }
}

void write_file(const fs::path& p, const std::vector<char>& data) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
}

void write_text(const fs::path& p, const std::string& text) {
    write_file(p, std::vector<char>(text.begin(), text.end()));
}

std::vector<char> read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<char>((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
}

// Every regular file under a, compared byte for byte with the same
// relative path under b, and the same file count on both sides.
bool same_tree(const fs::path& a, const fs::path& b) {
    std::size_t count_a = 0, count_b = 0;
    for (const auto& e : fs::recursive_directory_iterator(a)) {
        if (!e.is_regular_file()) continue;
        ++count_a;
        const auto rel = fs::relative(e.path(), a);
        if (!fs::exists(b / rel) || read_file(e.path()) != read_file(b / rel)) {
            return false;
        }
    }
    for (const auto& e : fs::recursive_directory_iterator(b)) {
        if (e.is_regular_file()) ++count_b;
    }
    return count_a == count_b;
}

template <typename Fn>
ErrorCode code_of(Fn fn) {
    try {
        fn();
    } catch (const std::exception& e) {
        return uatc::core::classify_exception(e);
    }
    return ErrorCode::Internal;   // "no error" sentinel for these tests
}

}  // namespace

int main() {
    using namespace uatc;

    const fs::path work = fs::absolute("uatc_archive_service_test_dir");
    fs::remove_all(work);

    // --- A tree: nested folders, an empty file, spaces, text, binary,
    //     and data larger than one block.
    const fs::path tree = work / "tree";
    write_text(tree / "readme.txt", "hello world\n");
    write_text(tree / "empty.txt", "");
    write_text(tree / "docs" / "my notes.txt", std::string(5000, 'a'));
    {
        std::string log;
        for (int i = 0; i < 3000; ++i) log += "line " + std::to_string(i) + " ok\n";
        write_text(tree / "logs" / "deep" / "app.log", log);
    }
    {
        // Incompressible-looking bytes from a fixed integer hash: the test
        // only needs data the probe stores as-is, not real randomness.
        std::vector<char> random(70000);
        std::uint32_t x = 2463534242u;
        for (auto& c : random) {
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            c = static_cast<char>(x >> 24);
        }
        write_file(tree / "bin" / "random.bin", random);
    }

    core::CompressOptions options;
    options.block_size = 4096;

    // --- compress_path on a folder, with progress
    const fs::path archive = work / "tree.uatc";
    core::NullProgressSink sink;
    const auto result = core::compress_path(tree, archive, options, &sink);
    check(result.fileCount == 5, "compress_path: five files recorded",
          std::to_string(result.fileCount));
    check(sink.blocksSeen == result.blockCount && result.blockCount > 5,
          "compress_path: progress reported for every block");
    check(result.storedBlocks + result.compressedBlocks == result.blockCount,
          "compress_path: stored + compressed = blocks");
    check(result.ratio > 0.0 &&
              result.archiveBytes == fs::file_size(archive),
          "compress_path: archive size and ratio reported");

    // --- TC-INFO-01: info reads folder archives (it used to refuse them)
    {
        const auto info = ArchiveInspector::inspect(archive);
        check(info.fileCount == 5, "TC-INFO-01: file count");
        check(info.originalBytes == result.originalBytes,
              "TC-INFO-01: original bytes = sum of files");
        check(info.blockCount == result.blockCount, "TC-INFO-01: block count");
        check(info.storedBlocks == result.storedBlocks &&
                  info.compressedBlocks == result.compressedBlocks,
              "TC-INFO-01: stored/compressed counts match the compressor");
        check(info.files.size() == 5 && info.files[0].path == "bin/random.bin" &&
                  info.files[1].path == "docs/my notes.txt",
              "TC-INFO-01: file list in sorted order");
        const double expected =
            static_cast<double>(info.originalBytes) /
            static_cast<double>(info.archiveBytes);
        check(info.ratio == expected,
              "TC-INFO-01: ratio is original/archive, same as the compressor");
    }

    // --- TC-VER-01
    {
        const auto v = ArchiveVerifier::verify(archive);
        check(v.success && v.filesChecked == 5, "TC-VER-01: folder archive verifies",
              v.message);
    }

    // --- TC-ARC-03: extract_path restores the tree
    {
        const fs::path out = work / "restored";
        const auto summary = core::extract_path(archive, out);
        check(summary.files == 5 && summary.bytesWritten == result.originalBytes,
              "TC-ARC-03: five files, every byte written");
        check(same_tree(tree, out), "TC-ARC-03: restored tree is byte-identical");
    }

    // --- A single file still works through the same facade
    {
        const fs::path single = work / "single.uatc";
        const auto r = core::compress_path(tree / "readme.txt", single, options);
        check(r.fileCount == 1 && ArchiveInspector::inspect(single).fileCount == 1,
              "facade: single file -> single-file archive");
        const auto s = core::extract_path(single, work / "single_out");
        check(s.files == 1 &&
                  read_file(work / "single_out" / "readme.txt") ==
                      read_file(tree / "readme.txt"),
              "facade: single-file round trip");
        check(ArchiveVerifier::verify(single).success, "facade: single file verifies");
    }

    // --- TC-PAR-01: worker count does not change the folder archive
    {
        core::CompressOptions parallel = options;
        parallel.workers = 4;
        const fs::path archive4 = work / "tree4.uatc";
        core::compress_path(tree, archive4, parallel);
        check(read_file(archive) == read_file(archive4),
              "TC-PAR-01: 1 and 4 workers give byte-identical folder archives");
    }

    // --- The archive is never packed into itself
    {
        const fs::path inside = tree / "self.uatc";
        core::compress_path(tree, inside, options);
        const auto again = core::compress_path(tree, inside, options);   // exists now
        check(again.fileCount == 5, "facade: archive inside the folder is skipped");
        fs::remove(inside);
    }

    // --- Errors carry SAD codes
    {
        fs::create_directories(work / "empty_dir");
        check(code_of([&] {
                  core::compress_path(work / "empty_dir", work / "e.uatc", options);
              }) == ErrorCode::InvalidArgument,
              "facade: empty folder -> E_INVALID_ARGUMENT");
        check(code_of([&] {
                  core::compress_path(work / "missing", work / "m.uatc", options);
              }) == ErrorCode::NotFound,
              "facade: missing source -> E_NOT_FOUND");
        check(code_of([&] { core::extract_path(work / "missing.uatc", work / "x"); }) ==
                  ErrorCode::NotFound,
              "facade: missing archive -> E_NOT_FOUND");
    }

    // --- TC-INT-01: a flipped payload byte in a folder archive
    {
        const fs::path damaged = work / "damaged.uatc";
        fs::copy_file(archive, damaged);
        auto bytes = read_file(damaged);
        // The last payload byte sits just before the integrity region
        // (2 x SHA-256 + 12-byte footer).
        bytes[bytes.size() - 32 - 32 - 12 - 1] ^= 0x5A;
        {
            std::ofstream out(damaged, std::ios::binary | std::ios::trunc);
            out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        }
        const auto v = ArchiveVerifier::verify(damaged);
        check(!v.success && (v.code == ErrorCode::BlockChecksum ||
                             v.code == ErrorCode::ArchiveChecksum),
              "TC-INT-01: corruption -> checksum error code",
              uatc::core::error_code_name(v.code) + std::string(" ") + v.message);

        const fs::path out = work / "damaged_out";
        const auto code = code_of([&] { core::extract_path(damaged, out); });
        check(code == ErrorCode::BlockChecksum || code == ErrorCode::ArchiveChecksum,
              "TC-INT-01: extract refuses the damaged archive");
        check(!fs::exists(out) || fs::is_empty(out),
              "TC-INT-01: nothing written for a damaged archive");
    }

    // --- TC-ERR-03: not an archive at all
    {
        const auto v = ArchiveVerifier::verify(tree / "readme.txt");
        check(!v.success && v.code == ErrorCode::MalformedArchive,
              "TC-ERR-03: non-archive -> E_MALFORMED_ARCHIVE",
              uatc::core::error_code_name(v.code));
    }

    // --- TC-SEC-01 at archive level: a tampered path with a recomputed
    //     archive checksum. Every checksum is valid, so only the path rule
    //     stands between the archive and a write outside the destination.
    {
        const fs::path victim = work / "victim1.bin";      // 11 characters
        write_text(victim, std::string(2000, 'A'));
        const fs::path evil_archive = work / "evil.uatc";
        core::compress_path(victim, evil_archive, options);

        auto bytes = read_file(evil_archive);
        auto u32 = [&bytes](std::size_t at) {
            std::uint32_t v = 0;
            for (int i = 0; i < 4; ++i)
                v |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + i])) << (8 * i);
            return v;
        };
        auto u64 = [&bytes](std::size_t at) {
            std::uint64_t v = 0;
            for (int i = 0; i < 8; ++i)
                v |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[at + i])) << (8 * i);
            return v;
        };
        // Header: file_table_offset is the u64 at byte 16. File record:
        // file id u32, then path offset u64, then path length u32.
        const std::size_t file_table = u64(16);
        const std::size_t path_at = u64(file_table + 4);
        const std::size_t path_len = u32(file_table + 12);
        const std::string evil = "../evil.bin";
        check(path_len == evil.size(), "SEC-01 setup: path lengths match");
        std::copy(evil.begin(), evil.end(), bytes.begin() + static_cast<long>(path_at));
        // Integrity region: file SHA-256, archive SHA-256, 12-byte footer.
        // The archive digest covers everything before the region.
        const std::size_t body = bytes.size() - 32 - 32 - 12;
        const auto digest = core::sha256(
            reinterpret_cast<const std::uint8_t*>(bytes.data()), body);
        std::copy(digest.begin(), digest.end(), bytes.begin() + static_cast<long>(body + 32));
        write_file(evil_archive, bytes);

        const auto v = ArchiveVerifier::verify(evil_archive);
        check(!v.success && v.code == ErrorCode::PathTraversal,
              "TC-SEC-01: verify refuses a valid-checksum archive with ../ path",
              uatc::core::error_code_name(v.code) + std::string(" ") + v.message);
        const fs::path dest = work / "sec_dest";
        check(code_of([&] { core::extract_path(evil_archive, dest); }) ==
                  ErrorCode::PathTraversal,
              "TC-SEC-01: extract refuses it with E_PATH_TRAVERSAL");
        check(!fs::exists(work / "evil.bin") && !fs::exists(dest / ".." / "evil.bin"),
              "TC-SEC-01: nothing written outside the destination");
    }

    fs::remove_all(work);
    if (failures != 0) {
        std::cout << "archive_service_test: " << failures << " FAILURE(S)\n";
        return 1;
    }
    std::cout << "archive_service_test: PASS\n";
    return 0;
}
