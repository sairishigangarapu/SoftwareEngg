
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "core/error.hpp"

namespace uatc {

struct VerificationResult {
    bool success = false;
    std::uint64_t filesChecked = 0;
    std::uint64_t blocksChecked = 0;
    std::string message;
    // On failure, the SAD 4.3.4 code (E_BLOCK_CHECKSUM, E_ARCHIVE_CHECKSUM,
    // E_UNSUPPORTED_VERSION, E_MALFORMED_ARCHIVE, ...), so the CLI exit code
    // and the API error do not depend on parsing the message text.
    core::ErrorCode code = core::ErrorCode::Internal;
};

class ArchiveVerifier {
public:
    // Validates the archive by reading and checking its contents: the
    // metadata (inspector), then a full decode of every block with its
    // CRC32 and the SHA-256 integrity data. Works for single-file and folder
    // archives. Does not extract files to disk and never throws.
    static VerificationResult verify(
        const std::filesystem::path& archive_path
    );
};

}
