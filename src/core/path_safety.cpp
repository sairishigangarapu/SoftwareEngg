#include "core/path_safety.hpp"

namespace uatc::core {

bool is_safe_relative_path(const std::string& recorded_path) {
    if (recorded_path.empty()) return false;

    const std::filesystem::path candidate(recorded_path);

    // An absolute path ignores the destination entirely.
    if (candidate.is_absolute()) return false;

    // Drive letters and UNC roots on Windows, rejected everywhere for
    // consistency because an archive may be written on any OS.
    if (!candidate.root_name().empty()) return false;

    // No '..' anywhere: normalising 'a/b/../c' escapes nothing here, but a
    // leading '..' escapes the destination, and the distinction is not
    // worth the risk of getting the resolution order wrong.
    for (const auto& part : candidate) {
        if (part == "..") return false;
    }

    // The normalised form must still be relative. A path that normalises
    // to nothing ("." or "./") is not a file.
    const auto normalised = candidate.lexically_normal();
    if (normalised.is_absolute()) return false;
    if (normalised.empty() || normalised == ".") return false;

    return true;
}

std::filesystem::path safe_relative_path(const std::string& recorded_path) {
    if (!is_safe_relative_path(recorded_path)) {
        throw UatcError(ErrorCode::PathTraversal,
                        "The archive contains an unsafe file path: " +
                            recorded_path);
    }
    return std::filesystem::path(recorded_path).lexically_normal();
}

}  // namespace uatc::core
