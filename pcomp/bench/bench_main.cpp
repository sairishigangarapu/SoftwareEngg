// Usage: pc_bench [--root DIR] [--out FILE] [--block-size N] [--reps N] [--workers 1,2,4,max]
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include "pc/block_loop.hpp"
#include "pc/thread_pool.hpp"

namespace fs = std::filesystem;
using namespace pc;

int main(int argc, char** argv) {
  std::string root = "datasets/samples", out = "results/bench.jsonl", workers_arg = "1,2,4,max";
  size_t block = 1 << 20; int reps = 3;
  for (int i = 1; i + 1 < argc; i += 2) {
    std::string k = argv[i], v = argv[i + 1];
    if (k == "--root") root = v; else if (k == "--out") out = v;
    else if (k == "--block-size") block = std::stoull(v);
    else if (k == "--reps") reps = std::stoi(v);
    else if (k == "--workers") workers_arg = v;
    else { std::cerr << "unknown arg " << k << "\n"; return 2; }
  }
  unsigned hw = std::max(1u, std::thread::hardware_concurrency());
  std::vector<unsigned> workers;
  { std::stringstream ss(workers_arg); std::string tok;
    while (std::getline(ss, tok, ',')) workers.push_back(tok == "max" ? hw : (unsigned)std::stoul(tok)); }

  std::sort(workers.begin(), workers.end());
  workers.erase(std::unique(workers.begin(), workers.end()), workers.end());

  fs::create_directories(fs::path(out).parent_path());
  std::string tmp = fs::path(out).parent_path().string() + "/tmp";
  fs::create_directories(tmp);
  std::ofstream os(out);
  os << env_json() << "\n";

  RawCodec codec;
  LoopConfig cfg{block, &codec};
  auto files = sorted_files(root);
  int rc = 0;
  for (uint32_t fi = 0; fi < files.size(); ++fi) {
    const std::string& rel = files[fi];
    const std::string path = root + "/" + rel;
    auto slash = rel.find('/');
    BenchRecord base;
    base.dataset = slash == std::string::npos ? "misc" : rel.substr(0, slash);
    base.file = rel; base.codec = codec.name(); base.block_size = block;

    for (unsigned w : workers) {
      base.workers = w;
      if (w > 1 && !kParallelPathSafe) {  // Day 4 gate: measure only if safe
        BenchRecord r = base; r.phase = "all"; r.status = "skipped_parallel_not_implemented";
        os << to_json(r) << "\n";
        continue;
      }
      const std::string arc = tmp + "/a.pc", rest = tmp + "/r.out";
      compress_file(path, arc, cfg, fi);  // warm-up (page cache), discarded
      for (int rep = 0; rep < reps; ++rep) {
        BenchRecord c = base; c.phase = "compress"; c.rep = rep;
        c.t = compress_file(path, arc, cfg, fi); c.raw_bytes = c.t.bytes_in;
        BenchRecord d = base; d.phase = "decompress"; d.rep = rep;
        d.t = decompress_file(arc, rest, cfg); d.raw_bytes = d.t.bytes_out;
        if (rep == 0 && fnv1a_file(path) != fnv1a_file(rest)) {
          c.status = d.status = "verify_failed"; rc = 1;
        }
        os << to_json(c) << "\n" << to_json(d) << "\n";
        if (rep == 0)
          std::printf("%-28s w=%u  comp %8.1f MB/s  decomp %8.1f MB/s  (%s)\n", rel.c_str(), w,
                      mb_per_s(c.raw_bytes, c.t.wall_ns), mb_per_s(d.raw_bytes, d.t.wall_ns),
                      c.status.c_str());
      }
    }
  }
  fs::remove_all(tmp);
  std::printf("results -> %s\n", out.c_str());
  return rc;
}
