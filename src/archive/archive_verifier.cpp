
#include "archive/archive_verifier.hpp"

#include <exception>

#include "archive/archive_inspector.hpp"
#include "archive/archive_reader.hpp"

namespace uatc {

VerificationResult ArchiveVerifier::verify(
    const std::filesystem::path& archive_path
) {
    VerificationResult result;

    try {
        // Inspect the metadata first to establish the file/block counts.
        const ArchiveInfo info = ArchiveInspector::inspect(archive_path);

        result.filesChecked = info.fileCount;
        result.blocksChecked = info.blockCount;

        // Reading reconstructs the original bytes and validates each
        // block's decoded length and checksum.
        (void)ArchiveReader::read(archive_path);

        result.success = true;
        result.message = "Archive verification successful.";
    } catch (const std::exception& error) {
        result.success = false;
        result.message = error.what();
    }

    return result;
}

} // namespace uatc
