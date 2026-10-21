#pragma once

// Error codes shared by the core, the archive layer, the CLI and the API
// layer, with the exit codes from SAD 4.3.4.

#include <stdexcept>
#include <string>

namespace uatc::core {

enum class ErrorCode {
    InvalidArgument,    // 2
    NotFound,            // 3
    Permission,          // 4
    Io,                  // 5
    MalformedArchive,    // 10
    UnsupportedVersion,  // 11
    MissingBlock,        // 12
    BlockChecksum,       // 13
    ArchiveChecksum,     // 14
    PathTraversal,       // 15
    Internal             // 1
};

// The API-style name, e.g. "E_BLOCK_CHECKSUM".
const char* error_code_name(ErrorCode code);

// The process exit code the CLI uses for this error.
int error_exit_code(ErrorCode code);

class UatcError : public std::runtime_error {
 public:
    UatcError(ErrorCode code, const std::string& message)
        : std::runtime_error(message), code_(code) {}

    ErrorCode code() const { return code_; }

 private:
    ErrorCode code_;
};

}  // namespace uatc::core
