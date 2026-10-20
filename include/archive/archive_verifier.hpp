
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace uatc {

struct VerificationResult {
    bool success = false;
    std::uint64_t filesChecked = 0;
    std::uint64_t blocksChecked = 0;
    std::string message;
};

class ArchiveVerifier {
public:
    // Validates the archive by reading and checking its contents.
    // Does not extract files to disk.
    static VerificationResult verify(
        const std::filesystem::path& archive_path
    );
};

}
