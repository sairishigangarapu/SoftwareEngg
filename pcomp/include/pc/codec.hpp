#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
namespace pc {

// A codec must be a PURE function of (input bytes, fixed params): no globals, no clocks,
// no thread ids. That is what makes parallel output identical to serial output.
class Codec {
 public:
  virtual ~Codec() = default;
  virtual const char* name() const = 0;
  virtual void compress(const uint8_t* in, size_t n, std::vector<uint8_t>& out) const = 0;
  virtual void decompress(const uint8_t* in, size_t n, size_t raw_size,
                          std::vector<uint8_t>& out) const = 0;
};

// "Raw block path": stores the block unchanged. Placeholder until a real codec lands.
class RawCodec : public Codec {
 public:
  const char* name() const override { return "raw"; }
  void compress(const uint8_t* in, size_t n, std::vector<uint8_t>& out) const override {
    out.assign(in, in + n);
  }
  void decompress(const uint8_t* in, size_t n, size_t, std::vector<uint8_t>& out) const override {
    out.assign(in, in + n);
  }
};

}  // namespace pc
