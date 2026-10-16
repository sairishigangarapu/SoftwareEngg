#pragma once

// DAY 5 - EntropyCoder interface (Strategy pattern for entropy coding).
// A codec is a PURE function of (input bytes + fixed params) so that
// parallel output is identical to serial output (ADR-05).
#include "archive/archive_format.hpp"
#include "core/byte_buffer.hpp"
#include "core/codec_result.hpp"

namespace uatc::core {

class EntropyCoder {
 public:
    virtual ~EntropyCoder() = default;

    virtual CodecId id() const = 0;
    virtual const char* name() const = 0;

    // Encode input into a self-delimiting byte stream.
    virtual CodecResult encode(ByteSpan in) const = 0;

    // Decode, reconstructing orig_size bytes.
    virtual ByteBuffer decode(ByteSpan in, std::size_t orig_size) const = 0;
};

}  // namespace uatc::core
