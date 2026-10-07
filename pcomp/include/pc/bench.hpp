#pragma once
// DAY 1/2/5 - timing helper, environment capture, result serialization (JSON Lines).
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

#ifndef PC_BUILD_FLAGS
#define PC_BUILD_FLAGS "unknown"
#endif

namespace pc {

inline uint64_t now_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct ScopedAccum {  // adds elapsed ns to a counter on scope exit
  uint64_t& slot; uint64_t t0;
  explicit ScopedAccum(uint64_t& s) : slot(s), t0(now_ns()) {}
  ~ScopedAccum() { slot += now_ns() - t0; }
};

struct StageTimes {
  uint64_t read_ns = 0, process_ns = 0, write_ns = 0, wall_ns = 0;
  uint64_t bytes_in = 0, bytes_out = 0, blocks = 0;
};

struct BenchRecord {
  std::string dataset, file, codec, phase, status = "ok";
  unsigned workers = 1;
  uint64_t block_size = 0;
  int rep = 0;
  uint64_t raw_bytes = 0;  // uncompressed-side bytes (basis for throughput)
  StageTimes t;
};

inline double mb_per_s(uint64_t bytes, uint64_t ns) {
  return ns ? (bytes / 1e6) / (ns / 1e9) : 0.0;
}

inline std::string json_escape(const std::string& s) {
  std::string o;
  for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
  return o;
}

inline std::string env_json() {
  std::ostringstream o;
  const char* os =
#if defined(__linux__)
      "linux";
#elif defined(__APPLE__)
      "macos";
#elif defined(_WIN32)
      "windows";
#else
      "unknown";
#endif
  o << "{\"type\":\"env\",\"hw_threads\":" << std::thread::hardware_concurrency()
    << ",\"os\":\"" << os << "\",\"compiler\":\"" << json_escape(__VERSION__)
    << "\",\"build_flags\":\"" << json_escape(PC_BUILD_FLAGS)
    << "\",\"ptr_bits\":" << sizeof(void*) * 8 << "}";
  return o.str();
}

inline std::string to_json(const BenchRecord& r) {
  std::ostringstream o;
  o << std::fixed << std::setprecision(3);
  o << "{\"type\":\"result\",\"dataset\":\"" << json_escape(r.dataset) << "\",\"file\":\""
    << json_escape(r.file) << "\",\"codec\":\"" << r.codec << "\",\"phase\":\"" << r.phase
    << "\",\"status\":\"" << r.status << "\",\"workers\":" << r.workers
    << ",\"block_size\":" << r.block_size << ",\"rep\":" << r.rep
    << ",\"raw_bytes\":" << r.raw_bytes << ",\"bytes_in\":" << r.t.bytes_in
    << ",\"bytes_out\":" << r.t.bytes_out << ",\"blocks\":" << r.t.blocks
    << ",\"read_ms\":" << r.t.read_ns / 1e6 << ",\"process_ms\":" << r.t.process_ns / 1e6
    << ",\"write_ms\":" << r.t.write_ns / 1e6 << ",\"wall_ms\":" << r.t.wall_ns / 1e6
    << ",\"mb_per_s\":" << mb_per_s(r.raw_bytes, r.t.wall_ns) << "}";
  return o.str();
}

}  // namespace pc
