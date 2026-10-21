#include "core/error.hpp"

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

}  // namespace uatc::core
