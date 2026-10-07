#include "pc/block_loop.hpp"
#include <cstdio>
#include <stdexcept>

namespace pc {

namespace {
struct File {
  FILE* f;
  File(const std::string& p, const char* mode) : f(std::fopen(p.c_str(), mode)) {
    if (!f) throw std::runtime_error("cannot open " + p);
  }
  ~File() { if (f) std::fclose(f); }
};
void put_u64(FILE* f, uint64_t v) {  // explicit little-endian => deterministic on any host
  uint8_t b[8];
  for (int i = 0; i < 8; ++i) b[i] = uint8_t(v >> (8 * i));
  std::fwrite(b, 1, 8, f);
}
bool get_u64(FILE* f, uint64_t& v) {
  uint8_t b[8];
  if (std::fread(b, 1, 8, f) != 8) return false;
  v = 0;
  for (int i = 0; i < 8; ++i) v |= uint64_t(b[i]) << (8 * i);
  return true;
}
}  // namespace

BlockResult process_block(const Codec& codec, BlockJob&& job) {
  BlockResult r;
  r.file_index = job.file_index; r.block_index = job.block_index; r.seq = job.seq;
  r.raw_size = job.data.size();
  codec.compress(job.data.data(), job.data.size(), r.data);
  return r;
}

StageTimes compress_file(const std::string& in, const std::string& out,
                         const LoopConfig& cfg, uint32_t file_index) {
  StageTimes t;
  const uint64_t w0 = now_ns();
  File fi(in, "rb"), fo(out, "wb");
  ReorderBuffer rb;
  uint64_t seq = 0, off = 0;
  for (;;) {
    BlockJob job;
    job.file_index = file_index; job.block_index = seq; job.seq = seq; job.offset = off;
    job.data.resize(cfg.block_size);
    size_t n;
    { ScopedAccum s(t.read_ns); n = std::fread(job.data.data(), 1, cfg.block_size, fi.f); }
    if (n == 0) break;
    job.data.resize(n);
    off += n; ++seq; t.bytes_in += n;
    BlockResult r;
    { ScopedAccum s(t.process_ns); r = process_block(*cfg.codec, std::move(job)); }
    rb.push(std::move(r), [&](const BlockResult& b) {
      ScopedAccum s(t.write_ns);
      put_u64(fo.f, b.raw_size);
      put_u64(fo.f, b.data.size());
      std::fwrite(b.data.data(), 1, b.data.size(), fo.f);
      t.bytes_out += 16 + b.data.size();
    });
  }
  t.blocks = seq;
  t.wall_ns = now_ns() - w0;
  return t;
}

StageTimes decompress_file(const std::string& in, const std::string& out, const LoopConfig& cfg) {
  StageTimes t;
  const uint64_t w0 = now_ns();
  File fi(in, "rb"), fo(out, "wb");
  std::vector<uint8_t> payload, raw;
  for (;;) {
    uint64_t raw_size = 0, plen = 0;
    bool ok;
    { ScopedAccum s(t.read_ns);
      ok = get_u64(fi.f, raw_size) && get_u64(fi.f, plen);
      if (ok) {
        payload.resize(plen);
        if (std::fread(payload.data(), 1, plen, fi.f) != plen) throw std::runtime_error("truncated");
      } }
    if (!ok) break;
    t.bytes_in += 16 + plen;
    { ScopedAccum s(t.process_ns); cfg.codec->decompress(payload.data(), plen, raw_size, raw); }
    { ScopedAccum s(t.write_ns); std::fwrite(raw.data(), 1, raw.size(), fo.f); }
    t.bytes_out += raw.size(); ++t.blocks;
  }
  t.wall_ns = now_ns() - w0;
  return t;
}

uint64_t fnv1a_file(const std::string& path) {
  File f(path, "rb");
  uint64_t h = 1469598103934665603ULL;
  uint8_t buf[1 << 16];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f.f)) > 0)
    for (size_t i = 0; i < n; ++i) { h ^= buf[i]; h *= 1099511628211ULL; }
  return h;
}

}  // namespace pc
