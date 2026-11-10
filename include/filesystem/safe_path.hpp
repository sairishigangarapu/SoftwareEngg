
#ifndef UATC_SAFE_PATH_HPP
#define UATC_SAFE_PATH_HPP

#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace uatc {

inline std::filesystem::path safe_destination(
    const std::filesystem::path& output_root,
    const std::filesystem::path& relative_path
) {
    namespace fs = std::filesystem;

    std::string portable = relative_path.generic_string();
    for (char& character : portable) {
        if (character == '\\') {
            character = '/';
        }
    }

    if (portable.empty() || portable.front() == '/' ||
        (portable.size() >= 2 &&
         std::isalpha(static_cast<unsigned char>(portable[0])) &&
         portable[1] == ':')) {
        throw std::runtime_error("Unsafe output path.");
    }

    fs::path normalized_relative;
    std::size_t start = 0;
    while (start < portable.size()) {
        const std::size_t end = portable.find('/', start);
        const std::string component =
            portable.substr(start, end == std::string::npos
                                       ? std::string::npos
                                       : end - start);
        if (component.empty() || component == "." || component == ".." ||
            component.find(':') != std::string::npos) {
            throw std::runtime_error("Unsafe output path.");
        }
        normalized_relative /= fs::path(component);
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    const fs::path root =
        fs::absolute(output_root).lexically_normal();

    const fs::path destination =
        (root / normalized_relative).lexically_normal();

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
