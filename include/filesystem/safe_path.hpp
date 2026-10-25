
#ifndef UATC_SAFE_PATH_HPP
#define UATC_SAFE_PATH_HPP

#include <filesystem>
#include <stdexcept>

namespace uatc {

inline std::filesystem::path safe_destination(
    const std::filesystem::path& output_root,
    const std::filesystem::path& relative_path
) {
    namespace fs = std::filesystem;

    if (relative_path.empty() ||
        relative_path.is_absolute() ||
        relative_path.has_root_name() ||
        relative_path.has_root_directory()) {
        throw std::runtime_error("Unsafe output path.");
    }

    for (const auto& component : relative_path) {
        if (component == "." || component == "..") {
            throw std::runtime_error("Unsafe output path.");
        }
    }

    const fs::path root =
        fs::absolute(output_root).lexically_normal();

    const fs::path destination =
        (root / relative_path).lexically_normal();

    const fs::path relative_check =
        destination.lexically_relative(root);

    if (relative_check.empty() || relative_check.is_absolute()) {
        throw std::runtime_error(
            "Output path escapes destination directory."
        );
    }

    for (const auto& component : relative_check) {
        if (component == "..") {
            throw std::runtime_error(
                "Output path escapes destination directory."
            );
        }
    }

    return destination;
}

} // namespace uatc

#endif // UATC_SAFE_PATH_HPP
