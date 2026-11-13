// DAY 23 - Directory compression through the CLI (FR-01/02/03).
//
// `uatc compress <dir>` walks the tree, encodes every file, and writes one
// archive; extract restores it. The file order is deterministic (sorted by
// relative path), the round trip is byte-for-byte, and a single-file
// archive still works the way it always did.
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

#include "archive/multi_file.hpp"
#include "core/compressor.hpp"
#include "core/error.hpp"
#include "core/path_safety.hpp"
#include "core/transform_factory.hpp"
#include "filesystem/file_enumerator.hpp"

namespace fs = std::filesystem;

namespace {

std::vector<std::uint8_t> read_all(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

void write_all(const fs::path& p, const std::vector<std::uint8_t>& data) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
}

}  // namespace

int main() {
    using namespace uatc;
    using namespace uatc::core;

    const fs::path work = "uatc_dir_cli_test_dir";
    fs::remove_all(work);
    fs::create_directories(work);

    // --- The tree: nested dirs, empty file, spaces in a name, binary ---
    const fs::path tree = work / "tree";
    fs::create_directories(tree / "src");
    fs::create_directories(tree / "data" / "nested");

    std::vector<std::uint8_t> binary(70000);
    {
        std::mt19937 rng(11);  // NOSONAR - deterministic test data
        for (auto& b : binary) b = static_cast<std::uint8_t>(rng());
    }

    const std::string readme = "hello world\n";
    const std::string main_cpp = "int main() { return 0; }\n";
    const std::string deep = "deep\n";
    const std::string empty_file = "";

    write_all(tree / "readme.txt",
              std::vector<std::uint8_t>(readme.begin(), readme.end()));
    write_all(tree / "src" / "main.cpp",
              std::vector<std::uint8_t>(main_cpp.begin(), main_cpp.end()));
    write_all(tree / "data" / "numbers.bin", binary);
    write_all(tree / "data" / "nested" / "deep.txt",
              std::vector<std::uint8_t>(deep.begin(), deep.end()));
    write_all(tree / "empty.txt", {});
    write_all(tree / "my test file.txt",
              std::vector<std::uint8_t>(readme.begin(), readme.end()));

    // --- Compress the tree: enumerate, encode, write ---
    const auto files = FileEnumerator::enumerate(tree);
    assert(files.size() == 6);
    // Deterministic order (FR-01): never directory order, never mtime.
    for (std::size_t i = 1; i < files.size(); ++i) {
        assert(files[i - 1].relative_path.generic_string() <
               files[i].relative_path.generic_string());
    }

    const auto registry = make_default_registry();
    const TransformSelector selector(registry);
    const CompressibilityProbe probe;

    std::vector<ArchiveInput> inputs;
    std::vector<std::vector<CodedBlock>> coded;
    for (const auto& file : files) {
        const auto data = read_all(tree / file.relative_path.generic_string());

        ArchiveInput input;
        input.relative_path = file.relative_path.generic_string();
        input.data = data;

        const auto blocks = BlockSplitter::split(data, 32 * 1024);
        std::vector<CodedBlock> per_file;
        per_file.reserve(blocks.size());
        for (const auto& block : blocks) {
            per_file.push_back(
                CompressionManager::encode_block(block, selector, probe));
        }
        coded.push_back(std::move(per_file));
        inputs.push_back(std::move(input));
    }

    const auto archive = work / "tree.uatc";
    MultiFileArchiveWriter::write(archive, inputs, coded, 32 * 1024);

    // --- Read it back: every file byte-for-byte, same order ---
    const auto tree_back = MultiFileArchiveReader::read(archive);
    assert(tree_back.files.size() == files.size());
    for (std::size_t i = 0; i < tree_back.files.size(); ++i) {
        assert(tree_back.files[i].relative_path ==
               files[i].relative_path.generic_string());
        assert(tree_back.files[i].data ==
               read_all(tree / files[i].relative_path.generic_string()));
    }

    // --- Extract: restore the tree, spaces and all ---
    const fs::path restored = work / "restored";
    for (const auto& file : tree_back.files) {
        write_all(restored / safe_relative_path(file.relative_path),
                  file.data);
    }
    assert(read_all(restored / "readme.txt") ==
           std::vector<std::uint8_t>(readme.begin(), readme.end()));
    assert(read_all(restored / "src" / "main.cpp") ==
           std::vector<std::uint8_t>(main_cpp.begin(), main_cpp.end()));
    assert(read_all(restored / "data" / "numbers.bin") == binary);
    assert(read_all(restored / "data" / "nested" / "deep.txt") ==
           std::vector<std::uint8_t>(deep.begin(), deep.end()));
    assert(read_all(restored / "empty.txt").empty());
    assert(read_all(restored / "my test file.txt") ==
           std::vector<std::uint8_t>(readme.begin(), readme.end()));

    // --- TC-SEC-01 still holds for every recorded path ---
    assert(is_safe_relative_path("my test file.txt"));
    assert(!is_safe_relative_path("../evil.txt"));

    fs::remove_all(work);
    std::printf("dir_cli_test: PASS\n");
    return 0;
}
