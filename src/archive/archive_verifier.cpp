
#include "archive/archive_verifier.hpp"

#include <exception>

#include "archive/archive_inspector.hpp"
#include "archive/multi_file.hpp"

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

        // Reading validates every file, block checksum and integrity digest.
        (void)MultiFileArchiveReader::read(archive_path);

        result.success = true;
        result.message = "Archive verification successful.";
    } catch (const std::exception& error) {
        result.success = false;
        result.message = error.what();
    }

    return result;
}

} // namespace uatc
