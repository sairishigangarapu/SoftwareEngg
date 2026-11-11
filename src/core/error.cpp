#include "core/error.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <new>
#include <system_error>

namespace uatc::core {

const char* error_code_name(ErrorCode code) {
    switch (code) {
        case ErrorCode::InvalidArgument:
            return "E_INVALID_ARGUMENT";
        case ErrorCode::NotFound:
            return "E_NOT_FOUND";
        case ErrorCode::Permission:
            return "E_PERMISSION";
        case ErrorCode::Io:
            return "E_IO";
        case ErrorCode::MalformedArchive:
            return "E_MALFORMED_ARCHIVE";
        case ErrorCode::UnsupportedVersion:
            return "E_UNSUPPORTED_VERSION";
        case ErrorCode::MissingBlock:
            return "E_MISSING_BLOCK";
        case ErrorCode::BlockChecksum:
            return "E_BLOCK_CHECKSUM";
        case ErrorCode::ArchiveChecksum:
            return "E_ARCHIVE_CHECKSUM";
        case ErrorCode::PathTraversal:
            return "E_PATH_TRAVERSAL";
        case ErrorCode::Internal:
        default:
            return "E_INTERNAL";
    }
}

int error_exit_code(ErrorCode code) {
    switch (code) {
        case ErrorCode::InvalidArgument:
            return 2;
        case ErrorCode::NotFound:
            return 3;
        case ErrorCode::Permission:
            return 4;
        case ErrorCode::Io:
            return 5;
        case ErrorCode::MalformedArchive:
            return 10;
        case ErrorCode::UnsupportedVersion:
            return 11;
        case ErrorCode::MissingBlock:
            return 12;
        case ErrorCode::BlockChecksum:
            return 13;
        case ErrorCode::ArchiveChecksum:
            return 14;
        case ErrorCode::PathTraversal:
            return 15;
        case ErrorCode::Internal:
        default:
            return 1;
    }
}

namespace {

bool contains_ci(std::string haystack, std::string needle) {
    const auto lower = [](std::string& s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
    };
    lower(haystack);
    lower(needle);
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

ErrorCode classify_exception(const std::exception& error) {
    if (const auto* uatc_error = dynamic_cast<const UatcError*>(&error)) {
        return uatc_error->code();
    }
    if (const auto* fs_error =
            dynamic_cast<const std::filesystem::filesystem_error*>(&error)) {
        const auto code = fs_error->code();
        if (code == std::errc::permission_denied ||
            code == std::errc::operation_not_permitted ||
            code == std::errc::read_only_file_system) {
            return ErrorCode::Permission;
        }
        if (code == std::errc::no_such_file_or_directory) {
            return ErrorCode::NotFound;
        }
        return ErrorCode::Io;
    }
    if (dynamic_cast<const std::bad_alloc*>(&error) != nullptr) {
        return ErrorCode::Internal;
    }

    // The archive layer reports format problems as std::runtime_error with
    // fixed messages (src/archive/archive_reader.cpp, multi_file.cpp,
    // archive_inspector.cpp).
    const std::string message = error.what();
    if (contains_ci(message, "block checksum")) return ErrorCode::BlockChecksum;
    if (contains_ci(message, "checksum mismatch")) return ErrorCode::ArchiveChecksum;
    // Exact message only: "has no integrity footer ... unsupported
    // version" is a damaged or foreign file, not a newer archive.
    if (contains_ci(message, "unsupported uatc archive version")) {
        return ErrorCode::UnsupportedVersion;
    }
    if (contains_ci(message, "does not exist")) return ErrorCode::NotFound;
    if (contains_ci(message, "could not open") ||
        contains_ci(message, "failed to open") ||
        contains_ci(message, "failed to write")) {
        return ErrorCode::Io;
    }
    return ErrorCode::MalformedArchive;
}

}  // namespace uatc::core
