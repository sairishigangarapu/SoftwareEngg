#pragma once

// Archive-reader fuzz target (STP TC-NFR-03, NFR-03, FR-12).
//
// fuzz_one() takes arbitrary bytes, writes them as a .uatc file and runs
// every reader entry point on them: verify, info (inspect) and extract.
// It returns a failure description, or an empty string when the input was
// handled correctly. "Correctly" means:
//
//   1. nothing crashes (the sanitizer build turns memory errors into
//      crashes, so they are caught too);
//   2. the verifier never throws (its contract), and the inspector and
//      extractor only throw std::exception subclasses;
//   3. nothing hangs: each step finishes within kStepLimit;
//   4. extraction never writes outside the destination folder;
//   5. verify and extract agree: an archive the verifier rejects is not
//      extracted;
//   6. no silent wrong output: if `expected` is given (the original files
//      of the seed archive), anything that is extracted must equal them.
//      With `names_may_change` (an archive re-sealed with a recomputed
//      checksum, i.e. deliberately rewritten), file names may differ but
//      every file's content must still be one of the originals.
//
// Used by the mutation driver, archive_fuzz_test.cpp.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <system_error>
#include <vector>

#include "archive/archive_inspector.hpp"
#include "archive/archive_verifier.hpp"
#include "core/archive_service.hpp"
#include "core/error.hpp"

namespace uatc_fuzz {

namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;
// relative path -> file contents
using Tree = std::map<std::string, Bytes>;

constexpr auto kStepLimit = std::chrono::seconds(2);

// What happened to one input, for the end-of-run histogram.
struct Outcome {
    bool verified = false;          // the verifier accepted it
    bool extracted = false;         // extraction completed
    std::string code = "OK";        // E_* code of the verifier's verdict
};

inline Bytes read_bytes(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return Bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

inline void write_bytes(const fs::path& p, const std::uint8_t* data, std::size_t n) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(n));
}

// Every regular file under root, by path relative to root.
inline Tree read_tree(const fs::path& root) {
    Tree tree;
    std::error_code ec;
    if (!fs::exists(root, ec)) return tree;
    for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec)) {
            tree[fs::relative(it->path(), root, ec).generic_string()] = read_bytes(it->path());
        }
    }
    return tree;
}

// True when the only entries directly under sandbox are the ones listed.
inline bool sandbox_clean(const fs::path& sandbox, const std::vector<std::string>& allowed) {
    std::error_code ec;
    for (fs::directory_iterator it(sandbox, ec), end; !ec && it != end; it.increment(ec)) {
        const auto name = it->path().filename().string();
        bool ok = false;
        for (const auto& a : allowed) ok = ok || name == a;
        if (!ok) return false;
    }
    return true;
}

template <typename Fn>
std::string timed(const char* step, Fn&& fn) {
    const auto start = std::chrono::steady_clock::now();
    fn();
    if (std::chrono::steady_clock::now() - start > kStepLimit) {
        return std::string(step) + " took longer than the step limit (hang)";
    }
    return "";
}

// Runs every reader entry point on `data`. `sandbox` must be an empty,
// private folder; it is left empty again on return. `expected` may be null.
// The contents of a tree, ignoring names (sorted, so order is irrelevant).
inline std::vector<Bytes> contents_of(const Tree& tree) {
    std::vector<Bytes> out;
    for (const auto& [name, data] : tree) out.push_back(data);
    std::sort(out.begin(), out.end());
    return out;
}

inline std::string fuzz_one(const std::uint8_t* data, std::size_t size,
                            const fs::path& sandbox, const Tree* expected,
                            Outcome* outcome = nullptr,
                            bool names_may_change = false) {
    const fs::path archive = sandbox / "input.uatc";
    const fs::path dest = sandbox / "dest";
    write_bytes(archive, data, size);

    Outcome local;
    Outcome& out = outcome ? *outcome : local;
    std::string failure;

    // 1. verify: must never throw.
    uatc::VerificationResult verdict;
    failure = timed("verify", [&] {
        try {
            verdict = uatc::ArchiveVerifier::verify(archive);
        } catch (...) {
            failure = "verify threw (its contract is to never throw)";
        }
    });
    if (!failure.empty()) return failure;
    out.verified = verdict.success;
    out.code = verdict.success ? "OK" : uatc::core::error_code_name(verdict.code);

    // 2. inspect: std::exception only.
    failure = timed("inspect", [&] {
        try {
            (void)uatc::ArchiveInspector::inspect(archive);
        } catch (const std::exception&) {
            // rejected: fine
        } catch (...) {
            failure = "inspect threw a non-std::exception";
        }
    });
    if (!failure.empty()) return failure;

    // 3. extract: std::exception only, nothing outside dest, agrees with
    //    verify, and extracts only the original content.
    bool extracted = false;
    failure = timed("extract", [&] {
        try {
            (void)uatc::core::extract_path(archive, dest);
            extracted = true;
        } catch (const std::exception&) {
            // rejected: fine
        } catch (...) {
            failure = "extract threw a non-std::exception";
        }
    });
    if (!failure.empty()) return failure;
    out.extracted = extracted;

    if (!sandbox_clean(sandbox, {"input.uatc", "dest"})) {
        failure = "extraction wrote outside the destination folder";
    } else if (extracted && !verdict.success) {
        failure = "extract accepted an archive the verifier rejected (" +
                  out.code + ": " + verdict.message + ")";
    } else if (extracted && expected != nullptr) {
        const Tree got = read_tree(dest);
        const bool same = names_may_change
            ? contents_of(got) == contents_of(*expected)
            : got == *expected;
        if (!same) {
            failure = "silent wrong output: extracted files differ from the original";
        }
    }

    std::error_code ec;
    fs::remove_all(dest, ec);
    fs::remove(archive, ec);
    return failure;
}

}  // namespace uatc_fuzz
