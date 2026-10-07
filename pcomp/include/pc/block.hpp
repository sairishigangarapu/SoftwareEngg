#pragma once
// DAY 2 - Deterministic ordering + job/result structures.
// See docs/ORDERING.md for the written rules.
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace pc {

struct BlockJob {            // independent unit of work
  uint32_t file_index = 0;   // index into the sorted file list
  uint64_t block_index = 0;  // block number within the file
  uint64_t seq = 0;          // global output order, assigned ONLY by the producer
  uint64_t offset = 0;       // byte offset in the source file
  std::vector<uint8_t> data; // raw input bytes
};

struct BlockResult {
  uint32_t file_index = 0;
  uint64_t block_index = 0;
  uint64_t seq = 0;
  uint64_t raw_size = 0;
  std::vector<uint8_t> data; // codec output
};

// Rule 1: files are ordered by relative path, '/' separators, bytewise comparison.
// Never depends on directory iteration order, locale, or mtime.
inline std::vector<std::string> sorted_files(const std::filesystem::path& root) {
  std::vector<std::string> out;
  for (auto& e : std::filesystem::recursive_directory_iterator(root))
    if (e.is_regular_file())
      out.push_back(std::filesystem::relative(e.path(), root).generic_string());
  std::sort(out.begin(), out.end());  // std::string compares as unsigned bytes
  return out;
}

// Rule 2: blocks are fixed-size slices by offset. Block size comes from config only,
// NEVER from thread count or file size heuristics tied to hardware.

// Rule 3: results are emitted in seq order, whatever order they finish in.
// (Not thread-safe: in the parallel version only the single writer thread owns this.)
class ReorderBuffer {
 public:
  template <class Emit>
  void push(BlockResult r, Emit&& emit) {
    uint64_t s = r.seq;
    pending_.emplace(s, std::move(r));
    while (!pending_.empty() && pending_.begin()->first == next_) {
      emit(pending_.begin()->second);
      pending_.erase(pending_.begin());
      ++next_;
    }
  }
  bool empty() const { return pending_.empty(); }
 private:
  std::map<uint64_t, BlockResult> pending_;
  uint64_t next_ = 0;
};

}  // namespace pc
