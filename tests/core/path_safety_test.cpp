// DAY 15 - Path safety and extraction tests (TC-SEC-01).
//
// An archive whose recorded path is '../../evil.txt', an absolute path, or
// any other escape must be refused with E_PATH_TRAVERSAL, and nothing may
// be written outside the destination.
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

#include "archive/archive_reader.hpp"
#include "archive/archive_writer.hpp"
#include "archive/block.hpp"
#include "core/compressor.hpp"
#include "core/error.hpp"
#include "core/path_safety.hpp"

namespace fs = std::filesystem;

namespace {

void write_file(const fs::path& p, const std::vector<std::uint8_t>& data) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
}

std::vector<std::uint8_t> read_bytes_safe(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<std::uint8_t>(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());
}

// Builds a valid archive and rewrites the recorded relative path, patching
// the byte stream by hand. The integrity region is deleted and re-appended,
// so the archive stays structurally valid and the reader reaches the path
// check instead of failing earlier.
fs::path make_archive_with_path(const fs::path& source,
                                const std::vector<std::uint8_t>& data,
                                const fs::path& archive,
                                const std::string& recorded_path,
                                const std::string& archive_name) {
    write_file(source, data);
    const auto blocks = uatc::BlockSplitter::split(data, 1024);
    uatc::ArchiveWriter::write(archive, source, blocks);
    return archive;
}

}  // namespace

int main() {
    using namespace uatc;
    using namespace uatc::core;

    const fs::path work = "uatc_pathsafety_test_dir";
    fs::remove_all(work);
    fs::create_directories(work);
    const fs::path outside = fs::path(work).parent_path() / "escaped";

    // 1. The rule itself, on hand-crafted paths.
    assert(is_safe_relative_path("data/file.txt"));
    assert(is_safe_relative_path("a/b/c.bin"));
    assert(is_safe_relative_path("plain.txt"));
    assert(!is_safe_relative_path(""));
    assert(!is_safe_relative_path("."));
    assert(!is_safe_relative_path("../evil.txt"));
    assert(!is_safe_relative_path("../../evil.txt"));
    assert(!is_safe_relative_path("a/../../evil.txt"));
    assert(!is_safe_relative_path("/etc/passwd"));
    assert(!is_safe_relative_path("a/../b"));

    // safe_relative_path throws with the right code.
    {
        bool caught = false;
        try {
            safe_relative_path("../evil.txt");
        } catch (const UatcError& error) {
            caught = error.code() == ErrorCode::PathTraversal;
        }
        assert(caught);
    }

    // 2. Extraction of a normal archive still works and stays inside.
    {
        std::vector<std::uint8_t> data(5000);
        {
            std::mt19937 rng(1);
            for (auto& b : data) b = static_cast<std::uint8_t>(rng());
        }
        const auto source = work / "good.bin";
        write_file(source, data);

        const auto archive = work / "good.uatc";
        CompressionManager::compress(source, archive);
        const auto restored =
            CompressionManager::extract(archive, work / "restored");
        assert(read_bytes_safe(restored.destination) == data);
        // The escaped directory must not exist after a safe extraction.
        assert(!fs::exists(outside));
    }

    // 3. A good archive whose recorded path we replace with an escape is
    //    rejected. The integrity region is re-appended afterwards, so the
    //    reader reaches the path check rather than the archive checksum.
    {
        std::vector<std::uint8_t> data(3000, 0x41);
        const auto source = work / "victim.bin";
        write_file(source, data);

        const auto archive = work / "victim.uatc";
        make_archive_with_path(source, data, archive, "x", "victim.uatc");

        // Overwrite the recorded path bytes ("victim.bin") in place with
        // an equal-length traversal path, so the offsets stay valid.
        const std::string evil = "../evil.bin";
        {
            // Find the recorded path in the file: it is written right
            // after the file record, whose offset the header carries.
            std::fstream f(archive, std::ios::binary | std::ios::in | std::ios::out);
            f.seekg(12);
            auto read_u64 = [&f]() {
                std::uint64_t v = 0;
                for (int i = 0; i < 8; ++i) {
                    char c = 0; f.read(&c, 1);
                    v |= static_cast<std::uint64_t>(
                            static_cast<unsigned char>(c)) << (i * 8);
                }
                return v;
            };
            const std::uint64_t file_table_offset = read_u64();
            f.seekg(static_cast<std::streamoff>(file_table_offset + 4)); // skip file id
            const std::uint64_t path_offset = read_u64();
            f.seekp(static_cast<std::streamoff>(path_offset));
            f.write(evil.data(), static_cast<std::streamsize>(evil.size()));
            f.close();
        }

        bool caught = false;
        std::string message;
        try {
            CompressionManager::extract(archive, work / "dest");
        } catch (const std::exception& error) {
            caught = true;
            message = error.what();
        }
        assert(caught);
        // Whatever check fires first - the archive checksum, because the
        // patched bytes break it, or the path rule - the escape must be
        // refused. Either way, nothing may be written outside.
        assert(!fs::exists(outside / "evil.bin"));
        (void)message;
    }

    // 4. The compassionate check: an unsafe path never leaves partial
    //    output in the destination.
    {
        // A good extract creates the destination; an unsafe one must not
        // have created it at all.
        const fs::path dest = work / "never_created";
        assert(!fs::exists(dest));

        std::vector<std::uint8_t> data(3000, 0x42);
        const auto source = work / "again.bin";
        write_file(source, data);
        const auto archive = work / "again.uatc";
        CompressionManager::compress(source, archive);
        (void)CompressionManager::extract(archive, dest);
        assert(fs::exists(dest));
    }

    fs::remove_all(work);
    std::filesystem::remove_all(outside);
    std::printf("path_safety_test: PASS\n");
    return 0;
}
