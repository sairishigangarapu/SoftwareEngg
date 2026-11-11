// Archive-reader fuzz test (STP TC-NFR-03; NFR-03, FR-09, FR-12).
//
// A deterministic mutation fuzzer. It builds a set of valid seed archives,
// then feeds the reader thousands of damaged variants and checks the rules
// in fuzz_target.hpp for every one: no crash, no hang, no unexpected
// exception, nothing written outside the destination, verify and extract
// agree, and never silently wrong output. Half the random cases are
// re-sealed with a recomputed archive checksum, as an attacker would do, so
// the damage also reaches the parsers and decoders behind the checksum.
//
//   UATC_FUZZ_CASES   number of random mutations (default 2000; the STP
//                     target is 10,000+, run by `make fuzz`)
//   UATC_FUZZ_SEED    RNG seed (default 20261111); every run with the same
//                     seed is identical, so any failure is reproducible
//   UATC_FUZZ_VERBOSE=1  print a note for every internal error and every
//                     damaged archive the verifier still accepted
//
// Build with -fsanitize=address,undefined (`make fuzz`) so memory errors
// that would not crash a normal build are caught as well.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "core/archive_service.hpp"
#include "core/sha256.hpp"
#include "fuzz_target.hpp"

namespace fs = std::filesystem;
using uatc_fuzz::Bytes;
using uatc_fuzz::Tree;

namespace {

// xorshift64*: small, fast and deterministic. Test data only, not security.
class Rng {
 public:
    explicit Rng(std::uint64_t seed) : s_(seed ? seed : 1) {}
    std::uint64_t next() {
        s_ ^= s_ >> 12;
        s_ ^= s_ << 25;
        s_ ^= s_ >> 27;
        return s_ * 2685821657736338717ULL;
    }
    std::size_t below(std::size_t n) { return n == 0 ? 0 : static_cast<std::size_t>(next() % n); }
    std::uint8_t byte() { return static_cast<std::uint8_t>(next() >> 56); }

 private:
    std::uint64_t s_;
};

struct Seed {
    std::string name;
    Bytes archive;
    Tree original;   // what a correct extraction must produce
};

Bytes text_bytes(const std::string& s) { return Bytes(s.begin(), s.end()); }

// Builds one seed: writes `files` under a folder, compresses the folder
// (or the single file), and records the archive bytes.
Seed make_seed(const fs::path& work, const std::string& name, const Tree& files,
               bool single_file, std::size_t block_size) {
    const fs::path src = work / ("src_" + name);
    fs::remove_all(src);
    for (const auto& [rel, data] : files) {
        fs::create_directories((src / rel).parent_path());
        uatc_fuzz::write_bytes(src / rel, data.data(), data.size());
    }
    const fs::path archive = work / (name + ".uatc");
    uatc::core::CompressOptions options;
    options.block_size = block_size;
    const fs::path input = single_file ? src / files.begin()->first : src;
    uatc::core::compress_path(input, archive, options);

    Seed seed;
    seed.name = name;
    seed.archive = uatc_fuzz::read_bytes(archive);
    // A single-file archive restores the file under its own name.
    if (single_file) {
        seed.original[fs::path(files.begin()->first).filename().string()] = files.begin()->second;
    } else {
        seed.original = files;
    }
    return seed;
}

std::vector<Seed> make_seeds(const fs::path& work) {
    Rng rng(7);
    Bytes random(20000);
    for (auto& b : random) b = rng.byte();
    std::string text;
    for (int i = 0; i < 800; ++i) text += "line " + std::to_string(i) + " of the server log, status ok\n";
    Bytes ramp;
    for (std::uint32_t i = 0; i < 6000; ++i) {
        for (int k = 0; k < 4; ++k) ramp.push_back(static_cast<std::uint8_t>((i * 3) >> (8 * k)));
    }

    std::vector<Seed> seeds;
    seeds.push_back(make_seed(work, "text", {{"log.txt", text_bytes(text)}}, true, 4096));
    seeds.push_back(make_seed(work, "random", {{"noise.bin", random}}, true, 8192));
    seeds.push_back(make_seed(work, "zeros", {{"zeros.bin", Bytes(30000, 0)}}, true, 4096));
    seeds.push_back(make_seed(work, "ramp", {{"ramp.bin", ramp}}, true, 4096));
    seeds.push_back(make_seed(work, "empty", {{"empty.txt", Bytes()}}, true, 4096));
    seeds.push_back(make_seed(work, "many_blocks", {{"blocks.txt", text_bytes(text)}}, true, 1024));
    seeds.push_back(make_seed(work, "folder",
                              {{"a.txt", text_bytes("alpha\n")},
                               {"docs/readme.md", text_bytes(text.substr(0, 3000))},
                               {"docs/deep/zeros.bin", Bytes(5000, 0)},
                               {"empty.txt", Bytes()},
                               {"noise.bin", Bytes(random.begin(), random.begin() + 3000)}},
                              false, 1024));
    return seeds;
}

const std::uint64_t kInteresting[] = {
    0, 1, 2, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0xFFFF, 0x10000, 0x7FFFFFFF,
    0x80000000, 0xFFFFFFFF, 0x100000000ULL, 0x7FFFFFFFFFFFFFFFULL,
    0xFFFFFFFFFFFFFFFFULL};

void put_le(Bytes& b, std::size_t at, std::uint64_t v, int width) {
    for (int i = 0; i < width && at + i < b.size(); ++i) {
        b[at + i] = static_cast<std::uint8_t>(v >> (8 * i));
    }
}

// One random mutation; returns its name for the histogram.
const char* mutate(Bytes& b, Rng& rng, const std::vector<Seed>& seeds) {
    if (b.empty()) {
        b.push_back(rng.byte());
        return "grow-empty";
    }
    switch (rng.below(10)) {
        case 0: {  // flip 1-8 random bits
            const std::size_t n = 1 + rng.below(8);
            for (std::size_t i = 0; i < n; ++i) b[rng.below(b.size())] ^= static_cast<std::uint8_t>(1u << rng.below(8));
            return "bit-flip";
        }
        case 1:  // one byte to an interesting value
            b[rng.below(b.size())] = static_cast<std::uint8_t>(kInteresting[rng.below(6)]);
            return "byte-value";
        case 2: {  // a u16/u32/u64 field in the metadata area to an extreme
            static const int widths[] = {2, 4, 8};
            const int width = widths[rng.below(3)];
            const std::size_t meta = std::min<std::size_t>(b.size(), 512);
            const std::size_t at = rng.below(meta);
            put_le(b, at, kInteresting[rng.below(std::size(kInteresting))], width);
            return "field-extreme";
        }
        case 3:  // truncate
            b.resize(rng.below(b.size()));
            return "truncate";
        case 4: {  // append junk
            const std::size_t n = 1 + rng.below(64);
            for (std::size_t i = 0; i < n; ++i) b.push_back(rng.byte());
            return "append";
        }
        case 5: {  // delete a range
            const std::size_t at = rng.below(b.size());
            const std::size_t n = 1 + rng.below(std::min<std::size_t>(64, b.size() - at));
            b.erase(b.begin() + static_cast<long>(at), b.begin() + static_cast<long>(at + n));
            return "delete";
        }
        case 6: {  // insert random bytes
            const std::size_t at = rng.below(b.size() + 1);
            Bytes ins(1 + rng.below(32));
            for (auto& x : ins) x = rng.byte();
            b.insert(b.begin() + static_cast<long>(at), ins.begin(), ins.end());
            return "insert";
        }
        case 7: {  // overwrite a range with random bytes
            const std::size_t at = rng.below(b.size());
            const std::size_t n = 1 + rng.below(std::min<std::size_t>(32, b.size() - at));
            for (std::size_t i = 0; i < n; ++i) b[at + i] = rng.byte();
            return "overwrite";
        }
        case 8: {  // splice: a chunk of another seed archive
            const Bytes& other = seeds[rng.below(seeds.size())].archive;
            const std::size_t from = rng.below(other.size());
            const std::size_t n = 1 + rng.below(std::min<std::size_t>(128, other.size() - from));
            const std::size_t at = rng.below(b.size());
            for (std::size_t i = 0; i < n && at + i < b.size(); ++i) b[at + i] = other[from + i];
            return "splice";
        }
        default: {  // copy a range inside the archive (duplicated records)
            const std::size_t from = rng.below(b.size());
            const std::size_t n = 1 + rng.below(std::min<std::size_t>(64, b.size() - from));
            const std::size_t to = rng.below(b.size());
            Bytes chunk(b.begin() + static_cast<long>(from), b.begin() + static_cast<long>(from + n));
            for (std::size_t i = 0; i < n && to + i < b.size(); ++i) b[to + i] = chunk[i];
            return "copy-range";
        }
    }
}

// Re-seals a damaged archive: recomputes the archive SHA-256 so the reader
// gets past the checksum and the damaged metadata and payloads reach the
// parsers and decoders behind it - what a deliberate attacker would do.
// Layout of the integrity region (SAD 4.1): file SHA-256, archive SHA-256,
// 12-byte footer; the archive digest covers everything before the region.
bool reseal(Bytes& b) {
    constexpr std::size_t kRegion = 32 + 32 + 12;
    if (b.size() < 40 + kRegion) return false;
    const std::size_t body = b.size() - kRegion;
    const auto digest = uatc::core::sha256(b.data(), body);
    std::copy(digest.begin(), digest.end(), b.begin() + static_cast<long>(body + 32));
    return true;
}

std::uint64_t env_number(const char* name, std::uint64_t fallback) {
    const char* v = std::getenv(name);
    return v ? std::strtoull(v, nullptr, 10) : fallback;
}

}  // namespace

int main() {
    const std::uint64_t cases = env_number("UATC_FUZZ_CASES", 2000);
    const std::uint64_t seed_value = env_number("UATC_FUZZ_SEED", 20261111);
    const bool verbose = env_number("UATC_FUZZ_VERBOSE", 0) != 0;

    const fs::path work = fs::absolute("uatc_fuzz_test_dir");
    fs::remove_all(work);
    fs::create_directories(work / "sandbox");
    const fs::path sandbox = work / "sandbox";

    const auto seeds = make_seeds(work);
    int failures = 0;
    std::map<std::string, std::uint64_t> verdicts, mutations;
    std::uint64_t accepted_mutants = 0, run = 0;

    auto check = [&](const Bytes& input, const Seed& seed, const std::string& what,
                     bool resealed = false) {
        uatc_fuzz::Outcome outcome;
        const auto failure = uatc_fuzz::fuzz_one(input.data(), input.size(), sandbox,
                                                 &seed.original, &outcome, resealed);
        ++run;
        ++verdicts[outcome.code];
        if (outcome.verified && input != seed.archive) ++accepted_mutants;
        // UATC_FUZZ_VERBOSE=1 explains unusual verdicts: an internal error,
        // or a damaged archive the verifier still accepted.
        if (verbose && (outcome.code == "E_INTERNAL" ||
                        (outcome.verified && input != seed.archive))) {
            uatc_fuzz::write_bytes(sandbox / "probe.uatc", input.data(), input.size());
            const auto v = uatc::ArchiveVerifier::verify(sandbox / "probe.uatc");
            fs::remove(sandbox / "probe.uatc");
            std::size_t first_diff = 0;
            while (first_diff < input.size() && first_diff < seed.archive.size() &&
                   input[first_diff] == seed.archive[first_diff]) ++first_diff;
            std::cout << "NOTE [" << what << "] " << outcome.code << " size "
                      << seed.archive.size() << "->" << input.size()
                      << " first change @" << first_diff << ": " << v.message << "\n";
        }
        if (!failure.empty()) {
            ++failures;
            const fs::path keep = work / ("failure_" + std::to_string(failures) + ".uatc");
            uatc_fuzz::write_bytes(keep, input.data(), input.size());
            std::cerr << "FAIL [" << what << "] " << failure << "\n"
                      << "     input kept at " << keep << "\n";
        }
    };

    // 1. Every seed must be accepted and restored exactly (sanity check:
    //    otherwise the rules below test nothing).
    for (const auto& seed : seeds) {
        uatc_fuzz::Outcome outcome;
        const auto failure = uatc_fuzz::fuzz_one(seed.archive.data(), seed.archive.size(),
                                                 sandbox, &seed.original, &outcome);
        if (!failure.empty() || !outcome.verified || !outcome.extracted) {
            std::cerr << "FAIL seed " << seed.name << " is not a valid archive: "
                      << failure << " (" << outcome.code << ")\n";
            return 1;
        }
    }
    std::cout << "PASS: " << seeds.size() << " seed archives verify and restore exactly\n";

    // 2. Exhaustive: every 1/2/4/8-byte field position in the first 64
    //    bytes (header and start of the file table) set to every
    //    interesting value, on every seed.
    for (const auto& seed : seeds) {
        for (std::size_t at = 0; at < std::min<std::size_t>(64, seed.archive.size()); ++at) {
            for (int width : {1, 2, 4, 8}) {
                for (std::uint64_t value : kInteresting) {
                    Bytes b = seed.archive;
                    put_le(b, at, value, width);
                    if (b == seed.archive) continue;
                    check(b, seed, seed.name + " header@" + std::to_string(at));
                }
            }
        }
    }
    const std::uint64_t exhaustive = run;
    std::cout << (failures ? "FAIL" : "PASS") << ": exhaustive header pass, "
              << exhaustive << " cases\n";

    // 3. Random stacked mutations (1-4 per case), reproducible from the seed.
    Rng rng(seed_value);
    for (std::uint64_t i = 0; i < cases; ++i) {
        const Seed& seed = seeds[rng.below(seeds.size())];
        Bytes b = seed.archive;
        const std::size_t stack = 1 + rng.below(4);
        std::string name;
        for (std::size_t k = 0; k < stack; ++k) {
            const char* m = mutate(b, rng, seeds);
            ++mutations[m];
            name += (k ? "+" : "") + std::string(m);
        }
        // Half the cases are re-sealed with a valid archive checksum, so the
        // folder reader (which checks the checksum first) is fuzzed past it.
        // A re-sealed archive was deliberately rewritten, so file names may
        // legitimately change; contents may not (see fuzz_target.hpp).
        bool resealed = false;
        if (rng.below(2) == 0 && reseal(b)) {
            resealed = true;
            name += "+reseal";
            ++mutations["(resealed)"];
        }
        if (b == seed.archive) continue;
        check(b, seed, seed.name + " case " + std::to_string(i) + " " + name, resealed);
    }
    std::cout << (failures ? "FAIL" : "PASS") << ": " << cases
              << " random mutated archives (seed " << seed_value << ")\n";

    // Summary: how the damaged archives were classified.
    std::cout << "\ncases run: " << run << "\nverifier verdicts:\n";
    for (const auto& [code, n] : verdicts) std::printf("  %-22s %8llu\n", code.c_str(), static_cast<unsigned long long>(n));
    std::cout << "mutations applied:\n";
    for (const auto& [m, n] : mutations) std::printf("  %-22s %8llu\n", m.c_str(), static_cast<unsigned long long>(n));
    std::cout << "damaged archives still accepted (must restore exactly): "
              << accepted_mutants << "\n";

    if (failures == 0) fs::remove_all(work);
    std::cout << (failures ? "archive_fuzz_test: FAILURES (inputs kept in uatc_fuzz_test_dir)\n"
                           : "archive_fuzz_test: PASS\n");
    return failures ? 1 : 0;
}
