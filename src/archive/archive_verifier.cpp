#include "archive/archive_verifier.hpp"

#include <exception>

#include "archive/archive_inspector.hpp"
#include "archive/archive_reader.hpp"
#include "archive/multi_file.hpp"
#include "core/path_safety.hpp"

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

        // An archive whose recorded paths would escape the destination is
        // not safe to extract, so it is not "verified" either, even when
        // every checksum matches (TC-SEC-01; a tampered archive can carry
        // a recomputed checksum).
        for (const auto& file : info.files) {
            if (!core::is_safe_relative_path(file.path)) {
                throw core::UatcError(
                    core::ErrorCode::PathTraversal,
                    "The archive contains an unsafe file path: " + file.path);
            }
        }

        // Reading reconstructs the original bytes and validates each
        // block's decoded length and checksum, then the integrity data.
        // A folder archive goes through the multi-file reader, which
        // checks the same things for every file.
        if (info.fileCount > 1) {
            (void)MultiFileArchiveReader::read(archive_path);
        } else {
            (void)ArchiveReader::read(archive_path);
        }

        result.success = true;
        result.message = "Archive verification successful.";
    } catch (const std::exception& error) {
        result.success = false;
        result.message = error.what();
        result.code = core::classify_exception(error);
    }

    return result;
}

} // namespace uatc
